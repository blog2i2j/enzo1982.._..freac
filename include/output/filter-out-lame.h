 /* BonkEnc Audio Encoder
  * Copyright (C) 2001-2004 Robert Kausch <robert.kausch@bonkenc.org>
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the "GNU General Public License".
  *
  * THIS PACKAGE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
  * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. */

#ifndef _H_FILTER_OUT_LAME_
#define _H_FILTER_OUT_LAME_

#include "outputfilter.h"
#include <3rdparty/lame/lame.h>

class FilterOutLAME : public OutputFilter
{
	private:
		lame_global_flags	*lameFlags;

		Buffer<unsigned char>	 outBuffer;
		Buffer<signed short>	 samplesBuffer;
	public:
					 FilterOutLAME(bonkEncConfig *, bonkEncTrack *);
					~FilterOutLAME();

		bool			 Activate();
		bool			 Deactivate();

		int			 WriteData(unsigned char *, int);
};

#endif
