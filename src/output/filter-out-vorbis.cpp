 /* BonkEnc Audio Encoder
  * Copyright (C) 2001-2004 Robert Kausch <robert.kausch@bonkenc.org>
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the "GNU General Public License".
  *
  * THIS PACKAGE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
  * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. */

#include <output/filter-out-vorbis.h>
#include <dllinterfaces.h>

FilterOutVORBIS::FilterOutVORBIS(bonkEncConfig *config, bonkEncTrack *format) : OutputFilter(config, format)
{
	packageSize = 0;

	if (format->channels > 2)
	{
		QuickMessage("BonkEnc does not support more than 2 channels!", "Error", MB_OK, IDI_HAND);

		error = 1;

		return;
	}

	srand(clock());

	ex_vorbis_info_init(&vi);

	switch (currentConfig->vorbis_mode)
	{
		case 0:
			ex_vorbis_encode_init_vbr(&vi, format->channels, format->rate, ((double) currentConfig->vorbis_quality) / 100);
			break;
		case 1:
			ex_vorbis_encode_init(&vi, format->channels, format->rate, -1, currentConfig->vorbis_bitrate * 1000, -1);
			break;
	}

	ex_vorbis_comment_init(&vc);

	if (currentConfig->enable_tags)
	{
		String	 prevOutFormat = String::SetOutputFormat("UTF-8");

		ex_vorbis_comment_add_tag(&vc, "COMMENT", currentConfig->default_comment);

		if (format->artist != NIL || format->title != NIL)
		{
			if (format->title != NIL)
			{
				ex_vorbis_comment_add_tag(&vc, "TITLE", format->title);
			}

			if (format->artist != NIL)
			{
				ex_vorbis_comment_add_tag(&vc, "ARTIST", format->artist);
			}

			if (format->album != NIL)
			{
				ex_vorbis_comment_add_tag(&vc, "ALBUM", format->album);
			}

			if (format->track > 0)
			{
				if (format->track < 10)	ex_vorbis_comment_add_tag(&vc, "TRACKNUMBER", String("0").Append(String::FromInt(format->track)));
				else			ex_vorbis_comment_add_tag(&vc, "TRACKNUMBER", String::FromInt(format->track));
			}

			if (format->year > 0)
			{
				ex_vorbis_comment_add_tag(&vc, "DATE", String::FromInt(format->year));
			}

			if (format->genre != NIL)
			{
				ex_vorbis_comment_add_tag(&vc, "GENRE", format->genre);
			}
		}

		String::SetOutputFormat(prevOutFormat);
	}

	ex_vorbis_analysis_init(&vd, &vi);
	ex_vorbis_block_init(&vd, &vb);

	ex_ogg_stream_init(&os, rand());
}

FilterOutVORBIS::~FilterOutVORBIS()
{
}

bool FilterOutVORBIS::Activate()
{
	ogg_packet	 header;
	ogg_packet	 header_comm;
	ogg_packet	 header_code;


	ex_vorbis_analysis_headerout(&vd, &vc, &header, &header_comm, &header_code);

	ex_ogg_stream_packetin(&os, &header); /* automatically placed in its own page */

	ex_ogg_stream_packetin(&os, &header_comm);
	ex_ogg_stream_packetin(&os, &header_code);

	int	 dataLength = 0;

	do
	{
		int result = ex_ogg_stream_flush(&os, &og);

		if (result == 0) break;

		backBuffer.Resize(dataLength);

		memcpy(backBuffer, dataBuffer, dataLength);

		dataBuffer.Resize(dataLength + og.header_len + og.body_len);

		memcpy(dataBuffer, backBuffer, dataLength);
		memcpy(((unsigned char *) dataBuffer) + dataLength, og.header, og.header_len);
		memcpy(((unsigned char *) dataBuffer) + dataLength + og.header_len, og.body, og.body_len);

		dataLength += og.header_len;
		dataLength += og.body_len;
	}
	while (true);

	driver->WriteData(dataBuffer, dataLength);

	return true;
}

bool FilterOutVORBIS::Deactivate()
{
	ex_vorbis_analysis_wrote(&vd, 0);

	int	 dataLength = 0;

	while (ex_vorbis_analysis_blockout(&vd, &vb) == 1)
	{
		ex_vorbis_analysis(&vb, NULL);
		ex_vorbis_bitrate_addblock(&vb);

		while(ex_vorbis_bitrate_flushpacket(&vd, &op))
		{
			ex_ogg_stream_packetin(&os, &op);

			do
			{
				int	 result = ex_ogg_stream_pageout(&os, &og);

				if (result == 0) break;

				backBuffer.Resize(dataLength);

				memcpy(backBuffer, dataBuffer, dataLength);

				dataBuffer.Resize(dataLength + og.header_len + og.body_len);

				memcpy(dataBuffer, backBuffer, dataLength);
				memcpy(((unsigned char *) dataBuffer) + dataLength, og.header, og.header_len);
				memcpy(((unsigned char *) dataBuffer) + dataLength + og.header_len, og.body, og.body_len);

				dataLength += og.header_len;
				dataLength += og.body_len;

				if (ex_ogg_page_eos(&og)) break;
			}
			while (true);
		}
	}

	driver->WriteData(dataBuffer, dataLength);

	ex_ogg_stream_clear(&os);
	ex_vorbis_block_clear(&vb);
	ex_vorbis_dsp_clear(&vd);
	ex_vorbis_comment_clear(&vc);
	ex_vorbis_info_clear(&vi);

	return true;
}

int FilterOutVORBIS::WriteData(unsigned char *data, int size)
{
	int	 dataLength = 0;
	int	 samples_size = size / (format->bits / 8);

	float	**buffer = ex_vorbis_analysis_buffer(&vd, samples_size / format->channels);

	if (format->bits != 16)
	{
		samplesBuffer.Resize(size / (format->bits / 8));

		for (int i = 0; i < size / (format->bits / 8); i++)
		{
			if (format->bits == 8)	samplesBuffer[i] = (data[i] - 128) * 256;
			if (format->bits == 24) samplesBuffer[i] = (int) (data[3 * i] + 256 * data[3 * i + 1] + 65536 * data[3 * i + 2] - (data[3 * i + 2] & 128 ? 16777216 : 0)) / 256;
			if (format->bits == 32)	samplesBuffer[i] = (int) ((long *) data)[i] / 65536;
		}

		if (format->channels == 1)
		{
			for (int j = 0; j < samples_size; j++)
			{
				buffer[0][j] = ((((signed char *) (unsigned short *) samplesBuffer)[j * 2 + 1] << 8) | (0x00ff & ((signed char *) (unsigned short *) samplesBuffer)[j * 2 + 0])) / 32768.f;
			}
		}

		if (format->channels == 2)
		{
			for (int j = 0; j < samples_size / 2; j++)
			{
				buffer[0][j] = ((((signed char *) (unsigned short *) samplesBuffer)[j * 4 + 1] << 8) | (0x00ff & ((signed char *) (unsigned short *) samplesBuffer)[j * 4 + 0])) / 32768.f;
				buffer[1][j] = ((((signed char *) (unsigned short *) samplesBuffer)[j * 4 + 3] << 8) | (0x00ff & ((signed char *) (unsigned short *) samplesBuffer)[j * 4 + 2])) / 32768.f;
			}
		}
	}
	else
	{
		if (format->channels == 1)
		{
			for (int j = 0; j < samples_size; j++)
			{
				buffer[0][j] = ((((signed char *) data)[j * 2 + 1] << 8) | (0x00ff & ((signed char *) data)[j * 2 + 0])) / 32768.f;
			}
		}

		if (format->channels == 2)
		{
			for (int j = 0; j < samples_size / 2; j++)
			{
				buffer[0][j] = ((((signed char *) data)[j * 4 + 1] << 8) | (0x00ff & ((signed char *) data)[j * 4 + 0])) / 32768.f;
				buffer[1][j] = ((((signed char *) data)[j * 4 + 3] << 8) | (0x00ff & ((signed char *) data)[j * 4 + 2])) / 32768.f;
			}
		}
	}

	ex_vorbis_analysis_wrote(&vd, samples_size / format->channels);

	while (ex_vorbis_analysis_blockout(&vd, &vb) == 1)
	{
		ex_vorbis_analysis(&vb, NULL);
		ex_vorbis_bitrate_addblock(&vb);

		while(ex_vorbis_bitrate_flushpacket(&vd, &op))
		{
			ex_ogg_stream_packetin(&os, &op);

			do
			{
				int	 result = ex_ogg_stream_pageout(&os, &og);

				if (result == 0) break;

				backBuffer.Resize(dataLength);

				memcpy(backBuffer, dataBuffer, dataLength);

				dataBuffer.Resize(dataLength + og.header_len + og.body_len);

				memcpy(dataBuffer, backBuffer, dataLength);
				memcpy(((unsigned char *) dataBuffer) + dataLength, og.header, og.header_len);
				memcpy(((unsigned char *) dataBuffer) + dataLength + og.header_len, og.body, og.body_len);

				dataLength += og.header_len;
				dataLength += og.body_len;

				if (ex_ogg_page_eos(&og)) break;
			}
			while (true);
		}
	}

	driver->WriteData(dataBuffer, dataLength);

	return dataLength;
}
