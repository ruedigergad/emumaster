/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	(c) Copyright 2011, elemental
*/

#include "mp3player.h"

Mp3Player::Mp3Player(QObject *parent) :
	QObject(parent),
	m_started(false)
{
	gst_init(0, 0);
	m_pipeline = gst_element_factory_make("playbin2", "mp3file");
	if (!m_pipeline) {
		qDebug("Cannot initialize gstreamer");
		return;
	}
}

Mp3Player::~Mp3Player()
{
	if (m_pipeline) {
		gst_element_set_state(m_pipeline, GST_STATE_NULL);
		gst_object_unref(GST_OBJECT(m_pipeline));
	}
}

void Mp3Player::start(const QString &fileName, bool play, int offset)
{
	if (!m_pipeline)
		return;
	m_mp3Name = QString("file://%1").arg(fileName).toLocal8Bit();
	g_object_set(G_OBJECT(m_pipeline), "uri", m_mp3Name.constData(), NULL);
	m_started = true;

	if (play)
		resume();
	else
		pause();
	// wait for worker thread, needed to seek later
	gst_element_get_state(m_pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);

	if (offset)
		seek(offset);
}

void Mp3Player::stop()
{
	if (!m_pipeline || !m_started)
		return;
	gst_element_set_state(m_pipeline, GST_STATE_NULL);
	m_started = false;
}

void Mp3Player::pause()
{
	if (!m_pipeline || !m_started)
		return;
	gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
}

void Mp3Player::resume()
{
	if (!m_pipeline || !m_started)
		return;
	gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
}

void Mp3Player::seek(int pos)
{
	if (!m_pipeline || !m_started)
		return;
	gint64 pos64 = (gint64)pos * (gint64)1000000;
	gst_element_seek_simple(m_pipeline, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, pos64);
}

int Mp3Player::pos()
{
	if (!m_pipeline || !m_started)
		return 0;

	gint64 pos;
	GstFormat fmt = GST_FORMAT_TIME;
	gst_element_query_position(m_pipeline, &fmt, &pos);
	return pos / 1000000;
}
