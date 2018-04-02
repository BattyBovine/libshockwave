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
		float ScaleX = 1.0f;
		float ScaleY = 1.0f;
		float RotateSkew0 = 0.0f;
		float RotateSkew1 = 0.0f;
		float TranslateX = 0.0f;
		float TranslateY = 0.0f;
	};

	struct CXForm
	{
		float RedMultTerm = 1.0f;
		float GreenMultTerm = 1.0f;
		float BlueMultTerm = 1.0f;
		float AlphaMultTerm = 1.0f;
		int16_t RedAddTerm = 0;
		int16_t GreenAddTerm = 0;
		int16_t BlueAddTerm = 0;
		int16_t AlphaAddTerm = 0;
		bool IsModified() { return !(
			(this->RedAddTerm==0 && this->GreenAddTerm==0 && this->BlueAddTerm==0 && this->AlphaAddTerm==0) &&
			(this->RedMultTerm==1.0f && this->GreenMultTerm==1.0f && this->BlueMultTerm==1.0f && this->AlphaMultTerm==1.0f)
			); }
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
		float FocalPoint = 0.0f;
	};



	struct FillStyle
	{
		enum class Type { SOLID, LINEARGRADIENT=0x10, RADIALGRADIENT=0x12, FOCALRADIALGRADIENT=0x13, REPEATINGBITMAP=0x40, CLIPPEDBITMAP=0x41, NONSMOOTHEDREPEATINGBITMAP=0x42, NONSMOOTHEDCLIPPEDBITMAP=0x43 } StyleType;
		RGBA Color;
		Matrix GradientMatrix;
		Gradient Gradient;
		uint16_t BitmapId = 0;
		Matrix BitmapMatrix;
	};
	typedef std::vector<FillStyle> FillStyleArray;
	typedef std::map<uint16_t,FillStyleArray> FillStyleMap;

	struct LineStyle
	{
		enum class Type { LINESTYLE, LINESTYLE2 } StyleType;
		enum class Cap  { ROUND, NONE, SQUARE } StartCapStyle, EndCapStyle;
		enum class Join { ROUND, BEVEL, MITER } JoinStyle;
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
	typedef std::vector<LineStyle> LineStyleArray;
	typedef std::map<uint16_t,LineStyleArray> LineStyleMap;

	struct StyleChangeRecord
	{
		bool MoveDeltaFlag = false;
		bool FillStyle0Flag = false;
		bool FillStyle1Flag = false;
		bool LineStyleFlag = false;
		bool NewStylesFlag = false;
		float MoveDeltaX = 0.0f;
		float MoveDeltaY = 0.0f;
		uint16_t FillStyle0 = 0;
		uint16_t FillStyle1 = 0;
		uint16_t LineStyle = 0;
		uint8_t NumFillBits : 4;
		uint8_t NumLineBits : 4;
		uint16_t NumNewFillStyles = 0;
		uint16_t NumNewLineStyles = 0;
	};

	struct Point
	{
		float x = 0.0f;
		float y = 0.0f;
		void transform(Matrix m)
		{
			x = ( x*m.ScaleX ) + ( y*m.RotateSkew1 ) + m.TranslateX;
			y = ( x*m.RotateSkew0 ) + ( y*m.ScaleY ) + m.TranslateY;
		}
	};
	struct Vertex
	{
		Point anchor;
		Point control;
	};
	struct Shape
	{
		uint8_t layer = 0;
		uint16_t fill0 = 0;
		uint16_t fill1 = 0;
		uint16_t stroke = 0;
		bool closed = false;
		std::vector<Vertex> vertices;
	};
	typedef std::vector<Shape> ShapeList;
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
		CXForm colourtransform;
	};
	typedef std::map<uint16_t,Character> CharacterDict;
	typedef std::map<uint16_t,DisplayChar> DisplayList;
	typedef std::list<DisplayList> FrameList;

	struct Dictionary
	{
		FillStyleMap FillStyles;
		LineStyleMap LineStyles;
		CharacterDict CharacterList;
		FrameList Frames;

		uint8_t NumFillBits : 4;
		uint8_t NumLineBits : 4;
		uint16_t NewCharOffset = 0;
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
