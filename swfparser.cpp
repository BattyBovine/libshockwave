#include "swfparser.h"
using namespace SWF;

#include <cstdio>
#ifndef LIBSHOCKWAVE_DISABLE_ZLIB
#include <zlib.h>
#endif
#ifndef LIBSHOCKWAVE_DISABLE_LZMA
#define _LZMA_PROB32
#include "lzma/LzmaLib.h"
#endif

Error Parser::parse_swf_data(uint8_t *data, uint32_t bytes)
{
	if(!data)
		return Error::NULL_DATA;

	uint32_t length =
		data[Header::FILESIZE_32] |
		data[Header::FILESIZE_32+1]<<8 |
		data[Header::FILESIZE_32+2]<<16 |
		data[Header::FILESIZE_32+3]<<24;
	size_t datalength = (length-Header::LENGTH);

	if(data[Header::SIGNATURE+1] != 'W' || data[Header::SIGNATURE+2] != 'S')
		return Error::DATA_INVALID;

	Stream *swfstream;
	switch(data[Header::SIGNATURE]) {
	case 'C':	// zlib compression
	{
		#ifndef LIBSHOCKWAVE_DISABLE_ZLIB
		if(data[Header::VERSION] < 6)	return Error::COMPRESSION_VERSION_MISMATCH;	// invalid if below SWF6
		uLong zliblen = (uLong)(bytes-Header::LENGTH);
		uint8_t *swfdecompressed = (uint8_t*)malloc(datalength);
		int zliberror = uncompress2(swfdecompressed, (uLong*)&datalength, &data[Header::LENGTH], &zliblen);
		switch(zliberror) {
		case Z_ERRNO:			return Error::ZLIB_ERRNO;
		case Z_STREAM_ERROR:	return Error::ZLIB_STREAM_ERROR;
		case Z_DATA_ERROR:		return Error::ZLIB_DATA_ERROR;
		case Z_MEM_ERROR:		return Error::ZLIB_MEMORY_ERROR;
		case Z_BUF_ERROR:		return Error::ZLIB_BUFFER_ERROR;
		case Z_VERSION_ERROR:	return Error::ZLIB_VERSION_ERROR;
		}
		swfstream = new Stream(swfdecompressed, datalength);
		break;
		#else
		return Error::ZLIB_NOT_COMPILED;
		#endif
	}
	case 'Z':	// lzma compression
	{
		#ifndef LIBSHOCKWAVE_DISABLE_LZMA
		if(data[Header::VERSION] < 13)	return Error::COMPRESSION_VERSION_MISMATCH;	// invalid if below SWF13
		size_t lzmalen =
			data[Header::LENGTH] |
			data[Header::LENGTH+1]<<8 |
			data[Header::LENGTH+2]<<16 |
			data[Header::LENGTH+3]<<24;
		uint8_t *swfdecompressed = (uint8_t*)malloc(datalength);
		SRes lzmaerror = LzmaUncompress(swfdecompressed, &datalength, &data[Header::LZMA_LENGTH+LZMA_PROPS_SIZE], &lzmalen, &data[Header::LZMA_LENGTH], LZMA_PROPS_SIZE);
		switch(lzmaerror) {
		case SZ_ERROR_DATA:			return Error::LZMA_DATA_ERROR;
		case SZ_ERROR_MEM:			return Error::LZMA_MEM_ALLOC_ERROR;
		case SZ_ERROR_UNSUPPORTED:	return Error::LZMA_INVALID_PROPS;
		case SZ_ERROR_INPUT_EOF:	return Error::LZMA_UNEXPECTED_EOF;
		}
		swfstream = new Stream(swfdecompressed, datalength);
		break;
		#else
		return Error::LZMA_NOT_COMPILED;
		#endif
	}
	default:	// no compression
		swfstream = new Stream(&data[Header::LENGTH], datalength);
	}

	movieprops.dimensions = swfstream->readRECT();
	movieprops.framerate = swfstream->readFIXED8();
	movieprops.framecount = swfstream->readUI16();

	this->tag_loop(swfstream);

	if(swfstream)	delete swfstream;
	return Error::OK;
}

Error Parser::tag_loop(Stream *swfstream)
{
	RecordHeader rh = swfstream->readRECORDHEADER();
	uint16_t framecounter = 0;
	while(rh.tag != TagType::End) {
		switch(rh.tag) {
			case TagType::DefineShape:
			case TagType::DefineShape2:
			case TagType::DefineShape3:
			{
				uint16_t shapeid = swfstream->readUI16();
				Rect shapebounds = swfstream->readRECT();
				swfstream->readSHAPEWITHSTYLE(rh.tag);
				break;
			}
			case TagType::DefineShape4:
			{
				uint16_t shapeid = swfstream->readUI16();
				Rect shapebounds = swfstream->readRECT();
				Rect edgebounds = swfstream->readRECT();
				swfstream->readUB(5);	// Reserved
				bool usesfillwindingrule = swfstream->readUB(1);
				bool usesnonscalingstrokes = swfstream->readUB(1);
				bool usesscalingstrokes = swfstream->readUB(1);
				swfstream->readSHAPEWITHSTYLE(rh.tag);
				break;
			}
			case TagType::PlaceObject:
			{
				int readlength = swfstream->get_pos();
				uint16_t characterid = swfstream->readUI16();
				uint16_t depth = swfstream->readUI16();
				Matrix matrix = swfstream->readMATRIX();
				readlength = (swfstream->get_pos()-readlength);
				if((rh.length-readlength)>0)	swfstream->readCXFORM();
			}
			case TagType::PlaceObject2:
			{
				int readlength = swfstream->get_pos();
				bool placeflaghasclipactions = swfstream->readUB(1);
				bool placeflaghasclipdepth = swfstream->readUB(1);
				bool placeflaghasname = swfstream->readUB(1);
				bool placeflaghasratio = swfstream->readUB(1);
				bool placeflaghascolortransform = swfstream->readUB(1);
				bool placeflaghasmatrix = swfstream->readUB(1);
				bool placeflaghascharacter = swfstream->readUB(1);
				bool placeflagmove = swfstream->readUB(1);

				uint16_t depth = swfstream->readUI16();
				if(placeflaghascharacter)		swfstream->readUI16();
				if(placeflaghasmatrix)			swfstream->readMATRIX();
				if(placeflaghascolortransform)	swfstream->readCXFORMWITHALPHA();
				if(placeflaghasratio)			swfstream->readUI16();
				if(placeflaghasname)			swfstream->readSTRING();
				if(placeflaghasclipdepth)		swfstream->readUI16();
				//if(placeflaghasclipactions)		swfstream->readCLIPACTIONS();
				readlength = (swfstream->get_pos()-readlength);
				if((rh.length-readlength)>0)	swfstream->readBytesAligned(rh.length-readlength);
				break;
			}
			case TagType::SetBackgroundColor:
			{
				movieprops.bgcolour = swfstream->readRGB();
				break;
			}
			case TagType::FileAttributes:
			{
				uint8_t getbits = swfstream->readBits(1);	// Reserved
				getbits = swfstream->readBits(1);			// UseDirectBlit
				getbits = swfstream->readBits(1);			// UseGPU
				getbits = swfstream->readBits(1);			// HasMetadata
				getbits = swfstream->readBits(1);			// SWFFlagsAS3
				getbits = swfstream->readBits(1);			// SWFFlagsNoCrossDomainCache
				getbits = swfstream->readBits(1);			// Reserved
				getbits = swfstream->readBits(1);			// UseNetwork
				for(int i=1; i<rh.length; i++)				// Reserved bytes
					getbits = swfstream->readUI8();
				break;
			}
			case TagType::Metadata:
			{
				const char *xmldata = swfstream->readSTRING();
				break;
			}
			case TagType::ShowFrame:
				framecounter++;
			default:
				swfstream->readBytesAligned(rh.length);
		}
		rh = swfstream->readRECORDHEADER();
	}
	return Error::OK;
}



Stream::Stream(uint8_t *d, uint32_t len)
{
	assert(d!=NULL);
	data = (uint8_t*)malloc(len);
	datalength = len;
	memcpy(data, d, datalength);
	rewind();
	reset_bits_pending();
}

Stream::~Stream()
{
	if(data)	delete data;
}



RecordHeader Stream::readRECORDHEADER()
{
	RecordHeader rh;
	uint16_t fulltag = readUI16();
	rh.length = (fulltag & 0x003F);
	if(rh.length==0x3F)	rh.length = readUI32();
	rh.tag = fulltag>>6;
	return rh;
}

FillStyle Stream::readFILLSTYLE(uint16_t tag)
{
	FillStyle fs;
	fs.FillStyleType = readUI8();
	switch(fs.FillStyleType) {
	case 0x00:	// Solid
		if(tag>TagType::DefineShape2)	fs.Color = readRGBA();
		else							fs.Color = readRGB();
		break;
	case 0x10:	// Linear gradient
	case 0x12:	// Radial gradient
	case 0x13:	// Focal radial gradient
		readMATRIX();
		if(fs.FillStyleType==0x13)	readFOCALGRADIENT(tag);
		else						readGRADIENT(tag);
		break;
	case 0x40:	// Repeating bitmap
	case 0x41:	// Clipped bitmap
	case 0x42:	// Non-smoothed repeating bitmap
	case 0x43:	// Non-smoothed clipped bitmap
		readUI16();
		readMATRIX();
		break;
	}
	return fs;
}

LineStyle Stream::readLINESTYLE(uint16_t tag)
{
	LineStyle ls;
	ls.Width = readUI16() / 20.0f;
	if(tag>TagType::DefineShape2)	ls.Color = readRGBA();
	else							ls.Color = readRGB();
	return ls;
}

LineStyle2 Stream::readLINESTYLE2(uint16_t tag)
{
	LineStyle2 ls;
	ls.Width = readUI16() / 20.0f;
	ls.StartCapStyle = static_cast<LineStyle::Cap>(readUB(2));
	ls.JoinStyle = static_cast<LineStyle::Join>(readUB(2));
	ls.HasFillFlag = readUB(1);
	ls.NoHScaleFlag = readUB(1);
	ls.NoVScaleFlag = readUB(1);
	ls.PixelHintingFlag = readUB(1);
	readUB(5);	// Reserved
	ls.NoClose = readUB(1);
	ls.EndCapStyle = static_cast<LineStyle::Cap>(readUB(2));
	if(ls.JoinStyle==LineStyle::Join::MITER)
		ls.MiterLimitFactor = readFIXED8();
	ls.Color = readRGBA();
	if(ls.HasFillFlag)
		ls.FillType = readFILLSTYLE(tag);
	return ls;
}

ShapeRecordArray Stream::readSHAPEWITHSTYLE(uint16_t tag)
{
	uint16_t stylecount = readUI8();	// FillStyleCount
	if(stylecount == 0xFF && (tag>TagType::DefineShape))
		stylecount = readUI16();
	for(int i=0; i<stylecount; i++)
		dictionary.FillStyles.push_back(readFILLSTYLE(tag));
	stylecount = readUI8();				// LineStyleCount
	if(stylecount == 0xFF && (tag>TagType::DefineShape))
		stylecount = readUI16();
	for(int i=0; i<stylecount; i++)
		if(tag==TagType::DefineShape4)	dictionary.LineStyles.push_back(readLINESTYLE2(tag));
		else							dictionary.LineStyles.push_back(readLINESTYLE(tag));
	dictionary.NumFillBits = readUB(4);
	dictionary.NumLineBits = readUB(4);

	reset_bits_pending();
	uint8_t typeflag = readUB(1);
	uint8_t stateflags = readUB(5);
	ShapeRecordArray shape;
	while(!(typeflag==0x00 && stateflags==0x00)) {
		shape.push_back(readSHAPERECORD(tag, typeflag, stateflags));
		typeflag = readUB(1);
		stateflags = readUB(5);
	}
	return shape;
}

Gradient Stream::readGRADIENT(uint16_t tag)
{
	Gradient g;
	g.SpreadMode = readUB(2);
	g.InterpolationMode = readUB(2);
	uint8_t numgrads = readUB(4);
	for(int i=0; i<numgrads; i++)
		g.GradientRecords.push_back(readGRADRECORD(tag));
	return g;
}

FocalGradient Stream::readFOCALGRADIENT(uint16_t tag)
{
	FocalGradient fg;
	fg.SpreadMode = readUB(2);
	fg.InterpolationMode = readUB(2);
	uint8_t numgrads = readUB(4);
	for(int i=0; i<numgrads; i++)
		fg.GradientRecords.push_back(readGRADRECORD(tag));
	fg.FocalPoint = readFIXED8();
	return fg;
}

GradRecord Stream::readGRADRECORD(uint16_t tag)
{
	GradRecord gr;
	gr.Ratio = readUI8();
	if(tag>TagType::DefineShape2)	gr.Color = readRGBA();
	else							gr.Color = readRGB();
	return gr;
}

ShapeRecord Stream::readSHAPERECORD(uint16_t tag, uint8_t typeflag, uint8_t stateflags)
{
	if(typeflag) {	// Edge record
		uint8_t numbits = (stateflags&0x0F)+2;	// NumBits (+2)
		if(stateflags&0x10) {	// StraightFlag
			StraightEdgeRecord r;
			r.RecordType = ShapeRecord::Type::STRAIGHTEDGE;
			if(readUB(1)) {		// GeneralLineFlag
				r.DeltaX = readSB(numbits) / 20.0f;
				r.DeltaY = readSB(numbits) / 20.0f;
			} else {
				if(readUB(1)) {	// VertLineFlag
					r.DeltaX = 0.0f;
					r.DeltaY = readSB(numbits) / 20.0f;
				} else {
					r.DeltaX = readSB(numbits) / 20.0f;
					r.DeltaY = 0.0f;
				}
			}
			return r;
		} else {
			CurvedEdgeRecord r;
			r.RecordType = ShapeRecord::Type::CURVEDEDGE;
			r.ControlDeltaX = readSB(numbits) / 20.0f;
			r.ControlDeltaY = readSB(numbits) / 20.0f;
			r.AnchorDeltaX = readSB(numbits) / 20.0f;
			r.AnchorDeltaY = readSB(numbits) / 20.0f;
			return r;
		}
	} else {		// Style change record
		StyleChangeRecord r;
		r.RecordType = ShapeRecord::Type::STYLECHANGE;

		r.MoveDeltaX = r.MoveDeltaY = 0.0f;
		if(stateflags&0x01) {					// StateMoveTo
			uint8_t movebits = readUB(5);
			r.MoveDeltaX = readSB(movebits) / 20.0f;
			r.MoveDeltaY = readSB(movebits) / 20.0f;
		}

		r.FillStyle0 = r.FillStyle1 = 0;
		if(stateflags&0x02)						// StateFillStyle0
			r.FillStyle0 = readUB(dictionary.NumFillBits);
		if(stateflags&0x04)						// StateFillStyle1
			r.FillStyle1 = readUB(dictionary.NumFillBits);

		r.LineStyle = 0;
		if(stateflags&0x08)						// StateLineStyle
			r.LineStyle = readUB(dictionary.NumLineBits);

		if(stateflags&0x10) {					// StateNewStyles
			uint16_t stylecount = readUI8();	// FillStyleCount
			if(stylecount == 0xFF && ( tag>TagType::DefineShape ))
				stylecount = readUI16();
			for(int i=0; i<stylecount; i++)
				dictionary.FillStyles.push_back(readFILLSTYLE(tag));
			stylecount = readUI8();				// LineStyleCount
			if(stylecount == 0xFF && ( tag>TagType::DefineShape ))
				stylecount = readUI16();
			for(int i=0; i<stylecount; i++)
				dictionary.LineStyles.push_back(readLINESTYLE(tag));
			dictionary.NumFillBits = readUB(4);	// NumFillBits
			dictionary.NumLineBits = readUB(4);	// NumLineBits
		}
		return r;
	}
}



Rect Stream::readRECT()
{
	reset_bits_pending();
	uint8_t bits = readUB(5);
	Rect rect;
	rect.xmin = readSB(bits) / 20.0f;
	rect.xmax = readSB(bits) / 20.0f;
	rect.ymin = readSB(bits) / 20.0f;
	rect.ymax = readSB(bits) / 20.0f;
	return rect;
}

RGBA Stream::readRGB()
{
	RGBA c;
	c.r = readUI8();
	c.g = readUI8();
	c.b = readUI8();
	c.a = 0xFF;
	return c;
}

RGBA Stream::readRGBA()
{
	RGBA c;
	c.r = readUI8();
	c.g = readUI8();
	c.b = readUI8();
	c.a = readUI8();
	return c;
}

RGBA Stream::readARGB()
{
	RGBA c;
	c.a = readUI8();
	c.r = readUI8();
	c.g = readUI8();
	c.b = readUI8();
	return c;
}

Matrix Stream::readMATRIX()
{
	reset_bits_pending();
	Matrix m;
	m.ScaleX = m.ScaleY = 1.0f;			// Set this as the default scale
	uint8_t bits = readUB(1);			// HasScale
	if(bits) {
		bits = readUB(5);				// NScaleBits
		m.ScaleX = readFB(bits);		// ScaleX
		m.ScaleY = readFB(bits);		// ScaleY
	}
	m.RotateSkew0=m.RotateSkew1=0.0f;	// Set this as the default skew
	bits = readUB(1);					// HasRotate
	if(bits) {
		bits = readUB(5);				// NRotateBits
		m.RotateSkew0 = readFB(bits);	// RotateSkew0
		m.RotateSkew1 = readFB(bits);	// RotateSkew1
	}
	m.TranslateX = m.TranslateY = 0.0f;	// Set this as the default translate
	bits = readUB(5);					// NTranslateBits
	m.TranslateX = readSB(bits);		// TranslateX
	m.TranslateY = readSB(bits);		// TranslateY
	return m;
}

CXForm Stream::readCXFORM(bool alpha)
{
	reset_bits_pending();
	CXForm cx;
	cx.RedAddTerm = cx.GreenAddTerm = cx.BlueAddTerm = cx.AlphaAddTerm = 0;
	cx.RedMultTerm = cx.GreenMultTerm = cx.BlueMultTerm = cx.AlphaMultTerm = 1.0f;
	bool hasmultterms = readUB(1);
	bool hasaddterms = readUB(1);
	uint8_t nbits = readUB(4);
	if(hasmultterms) {
		cx.RedMultTerm = readFB(nbits);
		cx.GreenMultTerm = readFB(nbits);
		cx.BlueMultTerm = readFB(nbits);
		if(alpha)	cx.AlphaMultTerm = readFB(nbits);
	}
	if(hasaddterms) {
		cx.RedAddTerm = readSB(nbits);
		cx.GreenAddTerm = readSB(nbits);
		cx.BlueAddTerm = readSB(nbits);
		if(alpha)	cx.AlphaAddTerm = readSB(nbits);
	}
	return cx;
}

ClipActions Stream::readCLIPACTIONS()
{
	ClipActions ca;
	return ca;
}



int8_t Stream::readSI8()
{
	return readByte();
}

int16_t Stream::readSI16()
{
	return (int16_t)readBytesAligned(sizeof(int16_t));
}

int32_t Stream::readSI32()
{
	return (int32_t)readBytesAligned(sizeof(int32_t));
}

int64_t Stream::readSI64()
{
	return (int64_t)readBytesAligned(sizeof(int64_t));
}

uint8_t Stream::readUI8()
{
	return readByte();
}

uint16_t Stream::readUI16()
{
	return (uint16_t)readBytesAligned(sizeof(uint16_t));
}

uint32_t Stream::readUI32()
{
	return (uint32_t)readBytesAligned(sizeof(uint32_t));
}

uint64_t Stream::readUI64()
{
	return (uint64_t)readBytesAligned(sizeof(uint64_t));
}

float Stream::readFLOAT()
{
	return (float)readBytesAligned(sizeof(float));
}

float Stream::readFLOAT16()
{
	uint16_t float16 = readSI16();
	int8_t sign = 1;
	if((float16&0x8000) != 0)	sign = -1;
	uint8_t exponent = ( float16>>10 ) & 0x1F;
	uint16_t significand = float16 & 0x3FF;
	if(exponent==0) {
		if(significand==0)	return 0.0f;
		return (float)(sign * pow(2, 1-FLOAT16_EXPONENT_BASE) * (significand / 1024.0f));
	} else if(exponent==31) {
		if(significand==0)	return 0.0f;
		return 0.0f;
	}
	return (float)(sign * pow(2, exponent-FLOAT16_EXPONENT_BASE) * (1 + significand / 1024.0f));
}

double Stream::readDOUBLE()
{
	return (double)readBytesAligned(sizeof(double));
}

float Stream::readFIXED()
{
	return readSI32() / 65536.0f;
}

float Stream::readFIXED8()
{
	return readSI16() / 256.0f;
}

const char *Stream::readSTRING()
{
	char *s = new char[256];
	char newchar;
	unsigned int i=0;
	for(i; i==0 || newchar!=0x00; i++) {
		if(i%256==0) {
			char *snew = new char[i+256];
			memcpy(snew, s, i);
			delete s;
			s = snew;
		}
		newchar = readUI8();
		s[i] = newchar;
	}
	char *snew = new char[i];
	memcpy(snew, s, i);
	delete s;
	return snew;
}



int32_t Stream::readSB(uint8_t bits)
{
	uint8_t shift = 32-bits;
	return (readBits(bits) << shift) >> shift;
}

uint32_t Stream::readUB(uint8_t bits)
{
	return readBits(bits);
}

float Stream::readFB(uint8_t bits)
{
	return readSB(bits) / 65536.0f;
}



uint8_t inline Stream::readByte()
{
	reset_bits_pending();
	return data[pos++];
}

uint64_t Stream::readBytesAligned(uint8_t bytes)
{
	uint64_t returnval = 0;
	for(int i=0; i<bytes; i++)
		returnval |= (uint64_t)readByte()<<(i*8);
	return returnval;
}

uint32_t Stream::readBits(uint8_t bits)
{
	if(bits==0)	return 0;
	if((bits%8)==0 && bits_pending==0)	return (uint32_t)readBytesAligned((uint8_t)floor(bits/8));
	
	uint32_t returnval = 0;
	while(bits>0) {
		if(bits_pending > 0) {
			uint8_t take = (uint8_t)fmin(bits_pending, bits);
			if(bits_pending==take) {
				returnval = (returnval<<take) | partial_byte;
				partial_byte = 0;
			} else {
				uint8_t mask = bitmasks[take];
				uint8_t remainmask = bitmasks[bits_pending-take];
				uint8_t taken = ( ( partial_byte >> (bits_pending - take) ) & mask );
				returnval = (returnval<<take) | taken;
				partial_byte = partial_byte & remainmask;
			}
			if(take==bits_pending)	partial_byte = 0;
			bits_pending -= take;
			bits -= take;
			continue;
		}
		partial_byte = readByte();
		bits_pending = 8;
	}

	return returnval;
}

uint32_t Stream::readEncodedU32()
{
	uint32_t result = readByte();
	if((result&0x80) != 0) {
		result = (result&0x7F) | ( readByte()<<7);
		if((result&0x4000) != 0) {
			result = (result&0x3FFF) | ( readByte()<<14);
			if((result&0x200000) != 0) {
				result = (result&0x1FFFFF) | ( readByte()<<21);
				if((result&0x10000000) != 0) {
					result = (result&0xFFFFFFF) | ( readByte()<<28);
				}
			}
		}
	}
	return result;
}
