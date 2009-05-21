#if !defined(__SWL_WIN_VIEW__WGL_PRINT_CONTEXT__H_ )
#define __SWL_WIN_VIEW__WGL_PRINT_CONTEXT__H_ 1


#include "swl/winview/WglContextBase.h"


namespace swl {

//-----------------------------------------------------------------------------------
//  Print Context for OpenGL

class SWL_WIN_VIEW_API WglPrintContext: public WglContextBase
{
public:
	typedef WglContextBase base_type;

public:
	WglPrintContext(HDC printDC, const Region2<int>& drawRegion, const bool isAutomaticallyActivated = true);
	WglPrintContext(HDC printDC, const RECT& drawRect, const bool isAutomaticallyActivated = true);
	virtual ~WglPrintContext();

private:
	WglPrintContext(const WglPrintContext &);
	WglPrintContext & operator=(const WglPrintContext &);

public:
	/// swap buffers
	/*virtual*/ bool swapBuffer();
	/// resize the context
	/*virtual*/ bool resize(const int x1, const int y1, const int x2, const int y2);

	/// get the native context
	/*virtual*/ boost::any getNativeContext()  {  return isActivated() ? boost::any(&memDC_) : boost::any();  }
	/*virtual*/ const boost::any getNativeContext() const  {  return isActivated() ? boost::any(&memDC_) : boost::any();  }

protected :
	/// re-create an OpenGL display list
	/*virtual*/ bool doRecreateDisplayList()  {  return true;  }

private:
	/// activate the context
	/*virtual*/ bool activate();
	/// de-activate the context
	/*virtual*/ bool deactivate();

	bool createOffScreen();
	bool createOffScreenBitmap(const int colorBitCount, const int colorPlaneCount);
	void deleteOffScreen();

private:
	/// a target print context
	HDC printDC_;

	/// a buffered context
	HDC memDC_;
	/// a buffered bitmap
	HBITMAP memBmp_;
	/// an old bitmap
	HBITMAP oldBmp_;
    /// an off-screen surface generated by the DIB section
	void *dibBits_;
};

}  // namespace swl


#endif  // __SWL_WIN_VIEW__WGL_PRINT_CONTEXT__H_
