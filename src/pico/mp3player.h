/*
	Free for non-commercial use.
	For commercial use, separate licencing terms must be obtained.
	(c) Copyright 2011, elemental
*/

#ifndef PICOMP3PLAYER_H
#define PICOMP3PLAYER_H

#include <QObject>
#include <gst/gst.h>

class Mp3Player : public QObject
{
	Q_OBJECT
public:
	explicit Mp3Player(QObject *parent = 0);	
	~Mp3Player();
public slots:
	void start(const QString &fileName, bool play, int offset);
	void stop();
	void pause();
	void resume();
	void seek(int pos);
	int pos();
private:
	GstElement *m_pipeline;
	GstElement *m_filesrc;
	bool m_started;
	int m_requestedOffset;
	QByteArray m_mp3Name;
};

#endif // PICOMP3PLAYER_H
