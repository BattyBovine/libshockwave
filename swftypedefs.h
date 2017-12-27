#ifndef LIBSHOCKWAVE_SWF_TYPEDEFS_H
#define LIBSHOCKWAVE_SWF_TYPEDEFS_H

#include <cstdint>
#include <vector>
#include <list>
#include <map>

namespace SWF
{
	enum ShapeRecordType
	{
		ENDSHAPE,
		STYLECHANGE,
		STRAIGHTEDGE,
		CURVEDEDGE
	};

	enum GradSpreadMode
	{
		PAD,
		REFLECT,
		REPEAT
	};

	enum GradInterpMode
	{
		RGB,
		LINEAR_RGB
	};

	struct RecordHeader
	{
		uint16_t tag : 10;
		uint32_t length;
	};

	struct Rect
	{
		float xmin = 0.0f;
		float xmax = 0.0f;
		float ymin = 0.0f;
		float ymax = 0.0f;
	};

	struct RGBA
	{
		uint8_t r = 0;
		uint8_t g = 0;
		uint8_t b = 0;
		uint8_t a = 0;
	};

	struct Matrix
	{
		float ScaleX = 0.0f;
		float ScaleY = 0.0f;
		float RotateSkew0 = 0.0f;
		float RotateSkew1 = 0.0f;
		int32_t TranslateX = 0;
		int32_t TranslateY = 0;
	};

	struct CXForm
	{
		float RedMultTerm = 0.0f;
		float GreenMultTerm = 0.0f;
		float BlueMultTerm = 0.0f;
		float AlphaMultTerm = 0.0f;
		uint8_t RedAddTerm = 0;
		uint8_t GreenAddTerm = 0;
		uint8_t BlueAddTerm = 0;
		uint8_t AlphaAddTerm = 0;
	};

	struct GradRecord
	{
		uint8_t Ratio = 0;
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
		double FocalPoint = 0.0L;
	};



	struct FillStyle
	{
		uint8_t FillStyleType = 0;
		RGBA Color;
		Matrix GradientMatrix;
		Gradient Gradient;
		uint16_t BitmapId = 0;
		Matrix BitmapMatrix;
	};
	typedef std::list<FillStyle> FillStyleArray;

	struct LineStyle
	{
		enum class Type { LINESTYLE, LINESTYLE2 } StyleType : 1;
		enum class Cap  { ROUND, NONE, SQUARE } StartCapStyle : 2, EndCapStyle : 2;
		enum class Join { ROUND, BEVEL, MITER } JoinStyle : 2;
		float Width = 1.0f;
		RGBA Color;
		bool HasFillFlag : 1;
		bool NoHScaleFlag : 1;
		bool NoVScaleFlag : 1;
		bool PixelHintingFlag : 1;
		bool NoClose : 1;
		float MiterLimitFactor = 1.0f;
		FillStyle FillType;
	};
	typedef std::list<LineStyle> LineStyleArray;

	struct StyleChangeRecord
	{
		int16_t MoveDeltaX = 0;
		int16_t MoveDeltaY = 0;
		uint16_t FillStyle0 = 0;
		uint16_t FillStyle1 = 0;
		uint16_t LineStyle = 0;
		uint8_t NumFillBits : 4;
		uint8_t NumLineBits : 4;
	};

	struct Point
	{
		int32_t xtwips = 0;
		int32_t ytwips = 0;
		double xpix() { return (xtwips/20.0L); }
		double ypix() { return (ytwips/20.0L); }
		void transform(Matrix m)
		{
			xtwips = ( xtwips*m.ScaleX ) + ( ytwips*m.RotateSkew1 ) + m.TranslateX;
			ytwips = ( xtwips*m.RotateSkew0 ) + ( ytwips*m.ScaleY ) + m.TranslateY;
		}
	};
	struct Vertex
	{
		Point anchor;
		Point control;
	};
	struct Shape
	{
		uint16_t fill0 = 0;
		uint16_t fill1 = 0;
		uint16_t stroke = 0;
		bool closed : 1;
		bool clockwise : 1;
		std::list<Vertex> vertices;
		bool is_empty() { return !vertices.size(); }
	};
	typedef std::list<Shape> ShapeList;
	struct Character
	{
		Rect bounds;
		ShapeList shapes;
		bool is_empty() { return !shapes.size(); }
	};
	struct DisplayChar
	{
		uint16_t id;
		Matrix transform;
	};
	typedef std::map<uint16_t,Character> CharacterDict;
	typedef std::map<uint16_t,DisplayChar> DisplayList;
	typedef std::list<DisplayList> FrameList;

	struct Dictionary
	{
		uint8_t NumFillBits : 4;
		uint8_t NumLineBits : 4;
		FillStyleArray FillStyles;
		LineStyleArray LineStyles;
		CharacterDict CharacterList;
		FrameList Frames;
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
