#ifndef LIBSHOCKWAVE_SWF_PARSER_H
#define LIBSHOCKWAVE_SWF_PARSER_H

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cassert>

#include "swftypedefs.h"

#define FLOAT16_EXPONENT_BASE 15

namespace SWF
{

	enum Header
	{
		SIGNATURE,
		VERSION = 3,
		FILESIZE_32,
		LENGTH = 8,
		LZMA_LENGTH = 12
	};

	enum Error
	{
		OK,
		NULL_DATA,
		DATA_INVALID,
		COMPRESSION_VERSION_MISMATCH,

		ZLIB_NOT_COMPILED,
		ZLIB_ERRNO,
		ZLIB_STREAM_ERROR,
		ZLIB_DATA_ERROR,
		ZLIB_MEMORY_ERROR,
		ZLIB_BUFFER_ERROR,
		ZLIB_VERSION_ERROR,

		LZMA_NOT_COMPILED,
		LZMA_DATA_ERROR,
		LZMA_MEM_ALLOC_ERROR,
		LZMA_INVALID_PROPS,
		LZMA_UNEXPECTED_EOF
	};

	enum TagType
	{
		End								= 0,
		ShowFrame						= 1,
		DefineShape						= 2,
		PlaceObject						= 4,
		RemoveObject					= 5,
		DefineBits						= 6,
		DefineButton					= 7,
		JPEGTables						= 8,
		SetBackgroundColor				= 9,
		DefineFont						= 10,
		DefineText						= 11,
		DoAction						= 12,
		DefineFontInfo					= 13,
		DefineSound						= 14,
		StartSound						= 15,
		DefineButtonSound				= 17,
		SoundStreamHead					= 18,
		SoundStreamBlock				= 19,
		DefineBitsLossless				= 20,
		DefineBitsJPEG2					= 21,
		DefineShape2					= 22,
		DefineButtonCxform				= 23,
		Protect							= 24,
		PlaceObject2					= 26,
		RemoveObject2					= 28,
		DefineShape3					= 32,
		DefineText2						= 33,
		DefineButton2					= 34,
		DefineBitsJPEG3					= 35,
		DefineBitsLossless2				= 36,
		DefineEditText					= 37,
		DefineSprite					= 39,
		FrameLabel						= 43,
		SoundStreamHead2				= 45,
		DefineMorphShape				= 46,
		DefineFont2						= 48,
		ExportAssets					= 56,
		ImportAssets					= 57,
		EnableDebugger					= 58,
		DoInitAction					= 59,
		DefineVideoStream				= 60,
		VideoFrame						= 61,
		DefineFontInfo2					= 62,
		EnableDebugger2					= 64,
		ScriptLimits					= 65,
		SetTabIndex						= 66,
		FileAttributes					= 69,
		PlaceObject3					= 70,
		ImportAssets2					= 71,
		DefineFontAlignZones			= 73,
		CSMTextSettings					= 74,
		DefineFont3						= 75,
		SymbolClass						= 76,
		Metadata						= 77,
		DefineScalingGrid				= 78,
		DoABC							= 82,
		DefineShape4					= 83,
		DefineMorphShape2				= 84,
		DefineSceneAndFrameLabelData	= 86,
		DefineBinaryData				= 87,
		DefineFontName					= 88,
		StartSound2						= 89,
		DefineBitsJPEG4					= 90,
		DefineFont4						= 91,
		EnableTelemetry					= 93
	};

	class Stream
	{
		uint8_t *data;
		uint32_t datalength;
		uint8_t swfversion;
		uint32_t pos;
		uint8_t bits_pending;
		uint8_t partial_byte;
		const uint8_t bitmasks[9] = {0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF};

		Dictionary dictionary;

	public:
		Stream(uint8_t*,uint32_t);
		~Stream();
		void inline seek(uint32_t s){pos=s;}
		uint32_t inline get_pos(){return pos;};
		void inline rewind(){pos=0;}
		void inline reset_bits_pending(){bits_pending=0;}

		RecordHeader readRECORDHEADER();
		FillStyle readFILLSTYLE(uint16_t);
		LineStyle readLINESTYLE(uint16_t);
		LineStyle2 readLINESTYLE2(uint16_t);
		ShapeRecordArray readSHAPEWITHSTYLE(uint16_t);
		Gradient readGRADIENT(uint16_t);
		FocalGradient readFOCALGRADIENT(uint16_t);
		GradRecord readGRADRECORD(uint16_t);
		ShapeRecord readSHAPERECORD(uint16_t, uint8_t, uint8_t);

		Rect readRECT();
		RGBA readRGB();
		RGBA readRGBA();
		RGBA readARGB();
		Matrix readMATRIX();
		CXForm readCXFORM(bool alpha=false);
		CXForm inline readCXFORMWITHALPHA() { return readCXFORM(true); }
		ClipActions readCLIPACTIONS();

		int8_t inline readSI8();
		int16_t inline readSI16();
		int32_t inline readSI32();
		int64_t inline readSI64();
		uint8_t inline readUI8();
		uint16_t inline readUI16();
		uint32_t inline readUI32();
		uint64_t inline readUI64();
		float inline readFLOAT();
		float inline readFLOAT16();
		double inline readDOUBLE();
		float inline readFIXED();
		float inline readFIXED8();
		const char inline *readSTRING();

		int32_t readSB(uint8_t);
		uint32_t readUB(uint8_t);
		float readFB(uint8_t);

		uint8_t inline readByte();
		uint64_t readBytesAligned(uint8_t);
		uint32_t readBits(uint8_t);
		uint32_t readEncodedU32();
	};

	class Parser
	{
		Properties movieprops;

		Error tag_loop(Stream*);

	public:
		Error parse_swf_data(uint8_t*, uint32_t);
	};
	
}

#endif //LIBSHOCKWAVE_SWF_PARSER_H
