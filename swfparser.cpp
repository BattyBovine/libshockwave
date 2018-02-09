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

Error Parser::parse_swf_data(uint8_t *data, uint32_t bytes, const char *password)
{
	if(!data)
		return Error::SWF_NULL_DATA;

	size_t datalength = (
		data[Header::FILESIZE_32] |
		data[Header::FILESIZE_32+1]<<8 |
		data[Header::FILESIZE_32+2]<<16 |
		data[Header::FILESIZE_32+3]<<24
		) - Header::LENGTH;

	if(data[Header::SIGNATURE+1] != 'W' || data[Header::SIGNATURE+2] != 'S')
		return Error::SWF_DATA_INVALID;

	if(swfstream) delete swfstream;

	movieprops = new Properties();
	movieprops->version = (data[Header::VERSION]);
	switch(data[Header::SIGNATURE]) {
	case 'C':	// zlib compression
	{
		#ifndef LIBSHOCKWAVE_DISABLE_ZLIB
		//if(data[Header::VERSION] < 6)	return Error::SWF_COMPRESSION_VERSION_MISMATCH;	// invalid if below SWF6
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
		//if(data[Header::VERSION] < 13)	return Error::SWF_COMPRESSION_VERSION_MISMATCH;	// invalid if below SWF13
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

	movieprops->dimensions = swfstream->readRECT();
	movieprops->framerate = swfstream->readFIXED8();
	movieprops->framecount = swfstream->readUI16();

	return this->tag_loop(swfstream);
}

Error Parser::tag_loop(Stream *swfstream)
{
	DisplayList currentdisplaystack;
	dictionary = swfstream->get_dict();

	RecordHeader rh = swfstream->readRECORDHEADER();
	uint16_t framecounter = 0;
	while(rh.tag != TagType::End) {
		switch(rh.tag) {
			case TagType::DefineShape:
			case TagType::DefineShape2:
			case TagType::DefineShape3:
			{
				uint16_t characterid = swfstream->readUI16();
				Rect shapebounds = swfstream->readRECT();
				swfstream->readSHAPEWITHSTYLE(characterid, shapebounds, rh.tag);
				break;
			}
			case TagType::DefineShape4:
			{
				uint16_t characterid = swfstream->readUI16();
				Rect shapebounds = swfstream->readRECT();
				Rect edgebounds = swfstream->readRECT();
				swfstream->readUB(5);	// Reserved
				bool usesfillwindingrule = swfstream->readUB(1);
				bool usesnonscalingstrokes = swfstream->readUB(1);
				bool usesscalingstrokes = swfstream->readUB(1);
				swfstream->readSHAPEWITHSTYLE(characterid, shapebounds, rh.tag);
				break;
			}
			case TagType::PlaceObject:
			{
				int readlength = swfstream->get_pos();
				uint16_t characterid = swfstream->readUI16();
				uint16_t depth = swfstream->readUI16();
				Matrix matrix = swfstream->readMATRIX();
				readlength = (swfstream->get_pos()-readlength);
				CXForm colourxform;
				if((rh.length-readlength)>0)
					colourxform = swfstream->readCXFORM();

				if(currentdisplaystack[depth].id!=characterid) {
					DisplayChar character;
					character.id = characterid;
					currentdisplaystack[depth] = character;
				}
				currentdisplaystack[depth].transform = matrix;
				currentdisplaystack[depth].colourtransform = colourxform;

				break;
			}
			case TagType::PlaceObject2:
			case TagType::PlaceObject3:
			{
				int readlength = swfstream->get_pos();

				bool placeflaghasclipactions = swfstream->readUB(1);
				bool placeflaghasclipdepth = swfstream->readUB(1);
				bool placeflaghasname = swfstream->readUB(1);
				bool placeflaghasratio = swfstream->readUB(1);
				bool placeflaghascolourtransform = swfstream->readUB(1);
				bool placeflaghasmatrix = swfstream->readUB(1);
				bool placeflaghascharacter = swfstream->readUB(1);
				bool placeflagmove = swfstream->readUB(1);
				bool placeflagopaquebackground = false,	placeflaghasvisible = false,	placeflaghasimage = false,
					placeflaghasclassname = false,	placeflaghascacheasbitmap = false,	placeflaghasblendmode = false,
					placeflaghasfilterlist = false;
				if(rh.tag==TagType::PlaceObject3) {
					placeflagopaquebackground = swfstream->readUB(1);
					placeflaghasvisible = swfstream->readUB(1);
					placeflaghasimage = swfstream->readUB(1);
					placeflaghasclassname = swfstream->readUB(1);
					placeflaghascacheasbitmap = swfstream->readUB(1);
					placeflaghasblendmode = swfstream->readUB(1);
					placeflaghasfilterlist = swfstream->readUB(1);
				}

				uint16_t depth = swfstream->readUI16();
				const char *name = NULL;
				if(rh.tag==TagType::PlaceObject3 && (placeflaghasclassname || (placeflaghasimage && placeflaghascharacter)))
					name = swfstream->readSTRING();
				uint16_t characterid = 0;
				Matrix matrix;
				CXForm colourxform;
				if(placeflaghascharacter)		characterid = swfstream->readUI16();
				if(placeflaghasmatrix)			matrix = swfstream->readMATRIX();
				if(placeflaghascolourtransform)	colourxform = swfstream->readCXFORMWITHALPHA();
				if(placeflaghasratio)			swfstream->readUI16();
				if(placeflaghasname)			swfstream->readSTRING();
				if(placeflaghasclipdepth)		swfstream->readUI16();
				if(rh.tag==TagType::PlaceObject3) {
					if(placeflaghasfilterlist)		swfstream->readFILTERLIST();
					if(placeflaghasblendmode)		swfstream->readUI8();
					if(placeflaghascacheasbitmap)	swfstream->readUI8();
					if(placeflaghasvisible)			swfstream->readUI8();
					if(placeflaghasvisible)			swfstream->readRGBA();
				}
				//if(placeflaghasclipactions)		swfstream->readCLIPACTIONS();

				if(placeflaghascharacter) {
					DisplayChar character;
					character.id = characterid;
					currentdisplaystack[depth] = character;
				}
				if(placeflaghasmatrix)			currentdisplaystack[depth].transform = matrix;
				if(placeflaghascolourtransform)	currentdisplaystack[depth].colourtransform = colourxform;

				readlength = (swfstream->get_pos()-readlength);
				if((rh.length-readlength)>0)	swfstream->readBytesAligned(rh.length-readlength);
				break;
			}
			case TagType::RemoveObject:
			case TagType::RemoveObject2:
			{
				uint16_t characterid = 0;
				if(rh.tag==TagType::RemoveObject)	characterid = swfstream->readUI16();
				uint16_t depth = swfstream->readUI16();
				currentdisplaystack.erase(depth);
				break;
			}
			case TagType::DefineSceneAndFrameLabelData:
			{
				uint32_t scenecount = swfstream->readEncodedU32();
				for(uint32_t i=0; i<scenecount; i++) {
					uint32_t offset = swfstream->readEncodedU32();
					const char *name = swfstream->readSTRING();
				}
				uint32_t framelabelcount = swfstream->readEncodedU32();
				for(uint32_t i=0; i<framelabelcount; i++) {
					uint32_t framenum = swfstream->readEncodedU32();
					const char *name = swfstream->readSTRING();
				}
				break;
			}
			case TagType::SetBackgroundColor:
			{
				movieprops->bgcolour = swfstream->readRGB();
				break;
			}
			case TagType::FrameLabel:
			{
				int readlength = swfstream->get_pos();
				swfstream->readSTRING();	// Name
				readlength = (swfstream->get_pos()-readlength);
				if((rh.length-readlength)>0)	swfstream->readUI8();	// Named Anchor Flag
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
			case TagType::Protect:
			{
				if(rh.length>0)
					return Error::SWF_FILE_ENCRYPTED;
				//else
				//	return Error::SWF_FILE_PROTECTED;
				break;
			}
			case TagType::Metadata:
			{
				const char *xmldata = swfstream->readSTRING();
				break;
			}
			case TagType::ShowFrame:
				dictionary->Frames.push_back(currentdisplaystack);
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
	data = d;
	datalength = len;
	rewind();
	reset_bits_pending();

	dict = new Dictionary();
}

RecordHeader inline Stream::readRECORDHEADER()
{
	RecordHeader rh;
	uint16_t fulltag = readUI16();
	rh.length = (fulltag & 0x003F);
	if(rh.length==0x3F)	rh.length = readUI32();
	rh.tag = fulltag>>6;
	return rh;
}

FillStyle inline Stream::readFILLSTYLE(uint16_t tag)
{
	FillStyle fs;
	fs.StyleType = static_cast<FillStyle::Type>(readUI8());
	switch(fs.StyleType) {
	case FillStyle::Type::SOLID:
		if(tag>=TagType::DefineShape3)	fs.Color = readRGBA();
		else							fs.Color = readRGB();
		break;
	case FillStyle::Type::LINEARGRADIENT:
	case FillStyle::Type::RADIALGRADIENT:
		readMATRIX();
		readGRADIENT(tag);
		break;
	case FillStyle::Type::FOCALRADIALGRADIENT:
		readMATRIX();
		readFOCALGRADIENT(tag);
		break;
	case FillStyle::Type::REPEATINGBITMAP:
	case FillStyle::Type::CLIPPEDBITMAP:
	case FillStyle::Type::NONSMOOTHEDREPEATINGBITMAP:
	case FillStyle::Type::NONSMOOTHEDCLIPPEDBITMAP:
		readUI16();
		readMATRIX();
		break;
	}
	return fs;
}

uint16_t inline Stream::readFILLSTYLEARRAY(uint16_t characterid, uint16_t tag)
{
	uint16_t stylecount = readUI8();	// FillStyleCount
	if((stylecount==0xFF) && (tag>=TagType::DefineShape2))
		stylecount = readUI16();
	for(int i=0; i<stylecount; i++)
		dict->FillStyles[characterid].push_back(readFILLSTYLE(tag));
	return stylecount;
}

LineStyle inline Stream::readLINESTYLE(uint16_t tag)
{
	LineStyle ls;
	ls.Width = readUI16() / 20.0f;
	if(tag>=TagType::DefineShape3)	ls.Color = readRGBA();
	else							ls.Color = readRGB();
	ls.StartCapStyle = ls.EndCapStyle = LineStyle::Cap::ROUND;
	ls.JoinStyle = LineStyle::Join::ROUND;
	ls.HasFillFlag = ls.NoHScaleFlag = ls.NoVScaleFlag = ls.PixelHintingFlag = ls.NoClose = false;
	ls.MiterLimitFactor = 1.0f;
	return ls;
}

LineStyle inline Stream::readLINESTYLE2(uint16_t tag)
{
	LineStyle ls;
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
		ls.MiterLimitFactor = (readUI16()/256.0f);
	if(ls.HasFillFlag)
		ls.FillType = readFILLSTYLE(tag);
	else
		ls.Color = readRGBA();
	return ls;
}

uint16_t inline Stream::readLINESTYLEARRAY(uint16_t characterid, uint16_t tag)
{
	uint16_t stylecount = readUI8();	// LineStyleCount
	if(stylecount==0xFF)
		stylecount = readUI16();
	for(int i=0; i<stylecount; i++) {
		if(tag>=TagType::DefineShape4)	dict->LineStyles[characterid].push_back(readLINESTYLE2(tag));
		else							dict->LineStyles[characterid].push_back(readLINESTYLE(tag));
	}
	return stylecount;
}

void inline Stream::readSHAPEWITHSTYLE(uint16_t characterid, Rect bounds, uint16_t tag)
{
	uint16_t fillbase = 0;	// Offsets for when new fill styles are found mid-shape
	uint16_t linebase = 0;

	readFILLSTYLEARRAY(characterid, tag);
	readLINESTYLEARRAY(characterid, tag);
	dict->NumFillBits = readUB(4);
	dict->NumLineBits = readUB(4);

	uint8_t typeflag = readUB(1);
	uint8_t stateflags = readUB(5);
	Character character;
	Shape shape;
	Point penlocation;
	while(!(typeflag==0x00 && stateflags==0x00)) {
		if(typeflag) {
			Vertex v = readSHAPERECORDedge((stateflags&0x10) ? ShapeRecordType::STRAIGHTEDGE : ShapeRecordType::CURVEDEDGE, (stateflags&0x0F)+2);
			v.anchor.x += penlocation.x;
			v.anchor.y += penlocation.y;
			v.control.x += penlocation.x;
			v.control.y += penlocation.y;
			shape.vertices.push_back(v);
			penlocation.x = v.anchor.x;
			penlocation.y = v.anchor.y;
		} else {
			StyleChangeRecord change = readSHAPERECORDstylechange(characterid, tag, stateflags);
			if(!shape.is_empty() && shape.vertices.size()>1)
				character.shapes.push_back(path_postprocess(shape));
			if(change.NewStylesFlag) {
				fillbase = this->dict->FillStyles[characterid].size()-change.NumNewFillStyles;
				linebase = this->dict->LineStyles[characterid].size()-change.NumNewLineStyles;
			}
			if(change.MoveDeltaFlag) {
				penlocation.x = change.MoveDeltaX;
				penlocation.y = change.MoveDeltaY;
			}
			shape.vertices.clear();
			Vertex v;
			v.anchor = penlocation;
			if(change.FillStyle0Flag)
				shape.fill0 = (change.FillStyle0 + fillbase);
			if(change.FillStyle1Flag)
				shape.fill1 = (change.FillStyle1 + fillbase);
			if(change.LineStyleFlag)
				shape.stroke = (change.LineStyle + linebase);
			shape.vertices.push_back(v);
		}
		typeflag = readUB(1);
		stateflags = readUB(5);
	}
	if(!shape.is_empty() && shape.vertices.size()>1)
		character.shapes.push_back(path_postprocess(shape));
	if(!character.is_empty()) {
		character.bounds = bounds;
		dict->CharacterList[characterid] = character;
	}
}

Gradient inline Stream::readGRADIENT(uint16_t tag)
{
	reset_bits_pending();
	Gradient g;
	g.SpreadMode = readUB(2);
	g.InterpolationMode = readUB(2);
	uint8_t numgrads = readUB(4);
	for(int i=0; i<numgrads; i++)
		g.GradientRecords.push_back(readGRADRECORD(tag));
	return g;
}

FocalGradient inline Stream::readFOCALGRADIENT(uint16_t tag)
{
	reset_bits_pending();
	FocalGradient fg;
	fg.SpreadMode = readUB(2);
	fg.InterpolationMode = readUB(2);
	uint8_t numgrads = readUB(4);
	for(int i=0; i<numgrads; i++)
		fg.GradientRecords.push_back(readGRADRECORD(tag));
	fg.FocalPoint = readFIXED8();
	return fg;
}

GradRecord inline Stream::readGRADRECORD(uint16_t tag)
{
	GradRecord gr;
	gr.Ratio = readUI8();
	if(tag>TagType::DefineShape2)	gr.Color = readRGBA();
	else							gr.Color = readRGB();
	return gr;
}

StyleChangeRecord inline Stream::readSHAPERECORDstylechange(uint16_t characterid, uint16_t tag, uint8_t stateflags)
{
	StyleChangeRecord r;

	if(stateflags&0x01) {				// StateMoveTo
		uint8_t movebits = readUB(5);
		r.MoveDeltaX = (readSB(movebits)/20.0f);
		r.MoveDeltaY = (readSB(movebits)/20.0f);
		r.MoveDeltaFlag = true;
	}

	if(stateflags&0x02) {				// StateFillStyle0
		r.FillStyle0 = readUB(dict->NumFillBits);
		r.FillStyle0Flag = true;
	}
	if(stateflags&0x04) {				// StateFillStyle1
		r.FillStyle1 = readUB(dict->NumFillBits);
		r.FillStyle1Flag = true;
	}

	if(stateflags&0x08) {				// StateLineStyle
		r.LineStyle = readUB(dict->NumLineBits);
		r.LineStyleFlag = true;
	}

	if(stateflags&0x10) {				// StateNewStyles
		r.NumNewFillStyles = readFILLSTYLEARRAY(characterid, tag);
		r.NumNewLineStyles = readLINESTYLEARRAY(characterid, tag);
		dict->NumFillBits = readUB(4);	// NumFillBits
		dict->NumLineBits = readUB(4);	// NumLineBits
		r.NewStylesFlag = true;
	}

	return r;
}

Vertex inline Stream::readSHAPERECORDedge(ShapeRecordType type, uint8_t numbits)
{
	switch(type) {
		case ShapeRecordType::STRAIGHTEDGE:
		{
			Vertex delta;
			if(readUB(1)) {		// GeneralLineFlag
				delta.anchor.x = delta.control.x = (readSB(numbits)/20.0f);
				delta.anchor.y = delta.control.y = (readSB(numbits)/20.0f);
			} else {
				if(readUB(1))	// VertLineFlag
					delta.anchor.y = delta.control.y = (readSB(numbits)/20.0f);
				else
					delta.anchor.x = delta.control.x = (readSB(numbits)/20.0f);
			}
			return delta;
		}
		case ShapeRecordType::CURVEDEDGE:
		{
			Vertex delta;
			delta.control.x = (readSB(numbits)/20.0f);
			delta.control.y = (readSB(numbits)/20.0f);
			delta.anchor.x = delta.control.x + (readSB(numbits)/20.0f);
			delta.anchor.y = delta.control.y + (readSB(numbits)/20.0f);
			return delta;
		}
	}
}

void inline Stream::readFILTERLIST()
{
	uint8_t count = readUI8();
	for(uint8_t i=0; i<count; i++) {
		uint8_t filterid = readUI8();
		switch(filterid) {
			case 0:	// DROPSHADOWFILTER
			{
				readRGBA();
				readFIXED();
				readFIXED();
				readFIXED();
				readFIXED();
				readFIXED8();
				readUB(1);
				readUB(1);
				readUB(1);
				readUB(5);
				break;
			}
			case 1:	// BLURFILTER
			{
				readFIXED();
				readFIXED();
				readUB(5);
				readUB(3);	// Reserved
				break;
			}
			case 2:	// GLOWFILTER
			{
				readRGBA();
				readFIXED();
				readFIXED();
				readFIXED8();
				readUB(1);
				readUB(1);
				readUB(1);
				readUB(5);
				break;
			}
			case 3:	// BEVELFILTER
			{
				readRGBA();
				readRGBA();
				readFIXED();
				readFIXED();
				readFIXED();
				readFIXED8();
				readUB(1);
				readUB(1);
				readUB(1);
				readUB(1);
				readUB(4);
				break;
			}
			case 4:	// GRADIENTGLOWFILTER
			{
				uint8_t numcolors = readUI8();
				for(int j=0; j<numcolors; j++) {
					readRGBA();
					readUI8();
				}
				readFIXED();
				readFIXED();
				readFIXED();
				readFIXED();
				readFIXED8();
				readUB(1);
				readUB(1);
				readUB(1);
				readUB(1);
				readUB(4);
				numcolors = readUI8();
				for(int j=0; j<numcolors; j++) {
					readRGBA();
					readUI8();
				}
				readFIXED();
				readFIXED();
				readFIXED();
				readFIXED();
				readFIXED8();
				readUB(1);
				readUB(1);
				break;
			}
			case 5:	// CONVOLUTIONFILTER
			{
				uint8_t matrixx = readUI8();
				uint8_t matrixy = readUI8();
				readFLOAT();
				readFLOAT();
				for(int j=0; j<( matrixx*matrixy ); j++)
					readFLOAT();
				readRGBA();
				readUB(6);	// Reserved
				readUB(1);
				readUB(1);
				break;
			}
			case 6:	// COLORMATRIXFILTER
			{
				for(int j=0; j<20; j++)
					readFLOAT();
				break;
			}
			case 7:	// GRADIENTBEVELFILTER
			{
				readUB(1);
				readUB(1);
				readUB(4);
				break;
			}
		}
	}
}



Rect inline Stream::readRECT()
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

RGBA inline Stream::readRGB()
{
	RGBA c;
	c.r = readUI8();
	c.g = readUI8();
	c.b = readUI8();
	c.a = 0xFF;
	return c;
}

RGBA inline Stream::readRGBA()
{
	RGBA c;
	c.r = readUI8();
	c.g = readUI8();
	c.b = readUI8();
	c.a = readUI8();
	return c;
}

RGBA inline Stream::readARGB()
{
	RGBA c;
	c.a = readUI8();
	c.r = readUI8();
	c.g = readUI8();
	c.b = readUI8();
	return c;
}

Matrix inline Stream::readMATRIX()
{
	reset_bits_pending();
	Matrix m;
	uint8_t bits = readUB(1);			// HasScale
	if(bits) {
		bits = readUB(5);				// NScaleBits
		m.ScaleX = readFB(bits);		// ScaleX
		m.ScaleY = readFB(bits);		// ScaleY
	}
	bits = readUB(1);					// HasRotate
	if(bits) {
		bits = readUB(5);				// NRotateBits
		m.RotateSkew0 = readFB(bits);	// RotateSkew0
		m.RotateSkew1 = readFB(bits);	// RotateSkew1
	}
	bits = readUB(5);					// NTranslateBits
	m.TranslateX = readSB(bits)/20.0f;	// TranslateX
	m.TranslateY = readSB(bits)/20.0f;	// TranslateY
	return m;
}

CXForm inline Stream::readCXFORM(bool alpha)
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

//ClipActions inline Stream::readCLIPACTIONS()
//{
//	ClipActions ca;
//	return ca;
//}



int8_t inline Stream::readSI8()
{
	return readByte();
}

int16_t inline Stream::readSI16()
{
	return (int16_t)readBytesAligned(sizeof(int16_t));
}

int32_t inline Stream::readSI32()
{
	return (int32_t)readBytesAligned(sizeof(int32_t));
}

int64_t inline Stream::readSI64()
{
	return (int64_t)readBytesAligned(sizeof(int64_t));
}

uint8_t inline Stream::readUI8()
{
	return readByte();
}

uint16_t inline Stream::readUI16()
{
	return (uint16_t)readBytesAligned(sizeof(uint16_t));
}

uint32_t inline Stream::readUI32()
{
	return (uint32_t)readBytesAligned(sizeof(uint32_t));
}

uint64_t inline Stream::readUI64()
{
	return (uint64_t)readBytesAligned(sizeof(uint64_t));
}

float inline Stream::readFLOAT()
{
	return (float)readBytesAligned(sizeof(float));
}

float inline Stream::readFLOAT16()
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

double inline Stream::readDOUBLE()
{
	return (double)readBytesAligned(sizeof(double));
}

float inline Stream::readFIXED()
{
	return readSI32() / 65536.0f;
}

float inline Stream::readFIXED8()
{
	return readSI16() / 256.0f;
}

const char inline *Stream::readSTRING()
{
	char *s = new char[256];
	char newchar;
	unsigned int i=0;
	for(i; i==0 || newchar!=0x00; i++) {
		if(i%256==0) {
			char *snew = new char[i+256];
			memcpy(snew, s, i);
			delete [] s;
			s = snew;
		}
		newchar = readUI8();
		s[i] = newchar;
	}
	char *snew = new char[i];
	memcpy(snew, s, i);
	delete [] s;
	return snew;
}



int32_t inline Stream::readSB(uint8_t bits)
{
	uint32_t readbits = readBits(bits);
	if(readbits&(1<<(bits-1)))	readbits |= (0xFFFFFFFF<<bits);
	return (int32_t)readbits;
}

uint32_t inline Stream::readUB(uint8_t bits)
{
	return readBits(bits);
}

float inline Stream::readFB(uint8_t bits)
{
	return readSB(bits) / 65536.0f;
}



uint8_t inline Stream::readByte()
{
	reset_bits_pending();
	assert(pos!=datalength);
	return data[pos++];
}

uint64_t inline Stream::readBytesAligned(uint8_t bytes)
{
	uint64_t returnval = 0;
	for(int8_t i=0; i<bytes; i++)
		returnval |= (uint64_t)readByte()<<(i*8);
	return returnval;
}

uint64_t inline Stream::readBytesAlignedBigEndian(uint8_t bytes)
{
	uint64_t returnval = 0;
	for(int8_t i=bytes; i>0; i--)
		returnval |= (uint64_t)readByte()<<((i-1)*8);
	return returnval;
}

uint32_t inline Stream::readBits(uint8_t bits)
{
	if(bits==0)	return 0;
	if((bits%8)==0 && bits_pending==0)	return readBytesAlignedBigEndian(bits/8);
	
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

uint32_t inline Stream::readEncodedU32()
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



Shape inline Stream::path_postprocess(Shape s)
{
	if(s.vertices.size()<3)	return s;
	s.closed = true;
	if((!(round(s.vertices.front().anchor.x*100.0f)==round(s.vertices.back().anchor.x*100.0f) &&
		round(s.vertices.front().anchor.y*100.0f)==round(s.vertices.back().anchor.y*100.0f))))
		s.closed = false;
	if(!s.closed)	s.vertices.push_back(s.vertices.front());	// Force a complete shape temporarily to determine winding
	size_t vsize = s.vertices.size();
	Vertex *varray = &s.vertices[0];
	int32_t area = 0;
	for(uint16_t i=1; i<vsize; i++)
		area += int32_t(round((varray[i-1].anchor.x*varray[i].anchor.y)-(varray[i].anchor.x*varray[i-1].anchor.y)));
	if(area > 0)
		s.winding = Shape::Winding::CLOCKWISE;
	else if(area < 0)
		s.winding = Shape::Winding::COUNTERCLOCKWISE;
	else
		s.winding = Shape::Winding::NONE;
	if(!s.closed)	s.vertices.pop_back();	// Don't forget to remove that temp point
	return s;
}
