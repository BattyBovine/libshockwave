#ifndef LIBSHOCKWAVE_SWF_TYPEDEFS_H
#define LIBSHOCKWAVE_SWF_TYPEDEFS_H

#include <cstdint>
#include <list>

namespace SWF
{
	enum ShapeRecordType {
		ENDSHAPE,
		STYLECHANGE,
		STRAIGHTEDGE,
		CURVEDEDGE
	};

	struct RecordHeader
	{
		uint16_t tag : 10;
		uint32_t length;
	};

	struct Rect
	{
		float xmin;
		float xmax;
		float ymin;
		float ymax;
	};

	struct RGBA
	{
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;
	};

	struct Matrix
	{
		float ScaleX;
		float ScaleY;
		float RotateSkew0;
		float RotateSkew1;
		int32_t TranslateX;
		int32_t TranslateY;
	};

	struct CXForm
	{
		float RedMultTerm;
		float GreenMultTerm;
		float BlueMultTerm;
		float AlphaMultTerm;
		uint8_t RedAddTerm;
		uint8_t GreenAddTerm;
		uint8_t BlueAddTerm;
		uint8_t AlphaAddTerm;
	};

	struct ClipActions{};

	struct GradRecord
	{
		uint8_t Ratio;
		RGBA Color;
	};
	typedef std::list<GradRecord> GradRecordArray;
	struct Gradient
	{
		uint8_t SpreadMode : 2;
		uint8_t InterpolationMode : 2;
		GradRecordArray GradientRecords;
	};
	struct FocalGradient : public Gradient
	{
		float FocalPoint;
	};



	struct FillStyle
	{
		uint8_t FillStyleType;
		RGBA Color;
		Matrix GradientMatrix;
		Gradient Gradient;
		uint16_t BitmapId;
		Matrix BitmapMatrix;
	};
	typedef std::list<FillStyle> FillStyleArray;

	struct LineStyle
	{
		enum class Type { LINESTYLE, LINESTYLE2 } StyleType : 1;
		enum class Cap  { ROUND, NONE, SQUARE } StartCapStyle : 2, EndCapStyle : 2;
		enum class Join { ROUND, BEVEL, MITER } JoinStyle : 2;
		float Width;
		RGBA Color;
		bool HasFillFlag : 1;
		bool NoHScaleFlag : 1;
		bool NoVScaleFlag : 1;
		bool PixelHintingFlag : 1;
		bool NoClose : 1;
		float MiterLimitFactor;
		FillStyle FillType;
	};
	typedef std::list<LineStyle> LineStyleArray;

	struct StyleChangeRecord
	{
		float MoveDeltaX;
		float MoveDeltaY;
		uint16_t FillStyle0;
		uint16_t FillStyle1;
		uint16_t LineStyle;
		uint8_t NumFillBits : 4;
		uint8_t NumLineBits : 4;
	};

	struct Point
	{
		float x = 0.0f;
		float y = 0.0f;
	};
	struct Vertex
	{
		Point anchor;
		Point control;
	};
	struct Shape
	{
		uint16_t fill0;
		uint16_t fill1;
		uint16_t stroke;
		bool closed : 1;
		std::list<Vertex> vertices;
	};
	typedef std::list<Shape> ShapeArray;

	struct Dictionary
	{
		uint8_t NumFillBits : 4;
		uint8_t NumLineBits : 4;
		FillStyleArray FillStyles;
		LineStyleArray LineStyles;
		ShapeArray ShapeRecords;
	};

	struct Properties
	{
		uint8_t version;
		Rect dimensions;
		RGBA bgcolour;
		float framerate;
		uint16_t framecount;
	};

}

#endif	// LIBSHOCKWAVE_SWF_TYPEDEFS_H
