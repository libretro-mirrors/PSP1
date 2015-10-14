/*
 * Copyright (c) 2012 Sacha Refshauge
 *
 */
// Qt 4.7+ / 5.0+ implementation of the framework.
// Currently supports: Android, Symbian, Blackberry, Maemo/Meego, Linux, Windows, Mac OSX

#include <QApplication>
#include <QUrl>
#include <QDir>
#include <QDesktopWidget>
#include <QDesktopServices>
#include <QLocale>
#include <QThread>

#if QT_VERSION > QT_VERSION_CHECK(5, 0, 0)
#include <QStandardPaths>
#ifdef QT_HAS_SYSTEMINFO
#include <QScreenSaver>
#endif
#endif

#ifdef __SYMBIAN32__
#include <QSystemScreenSaver>
#include <QFeedbackHapticsEffect>
#include "SymbianMediaKeys.h"
#endif
#ifdef SDL
#include "SDL/SDLJoystick.h"
#include "SDL_audio.h"
#endif
#include "QtMain.h"
#include "math/math_util.h"

#include <string.h>

InputState* input_state;

#ifdef SDL
extern void mixaudio(void *userdata, Uint8 *stream, int len) {
	NativeMix((short *)stream, len / 4);
}
#endif

std::string System_GetProperty(SystemProperty prop) {
	switch (prop) {
	case SYSPROP_NAME:
#ifdef __SYMBIAN32__
		return "Qt:Symbian";
#elif defined(BLACKBERRY)
		return "Qt:Blackberry";
#elif defined(MAEMO)
		return "Qt:Maemo";
#elif defined(ANDROID)
		return "Qt:Android";
#elif defined(Q_OS_LINUX)
		return "Qt:Linux";
#elif defined(_WIN32)
		return "Qt:Windows";
#elif defined(Q_OS_MAC)
		return "Qt:Mac";
#else
		return "Qt";
#endif
	case SYSPROP_LANGREGION:
		return QLocale::system().name().toStdString();
	default:
		return "";
	}
}

int System_GetPropertyInt(SystemProperty prop) {
  switch (prop) {
  case SYSPROP_AUDIO_SAMPLE_RATE:
    return 44100;
	case SYSPROP_DISPLAY_REFRESH_RATE:
		return 60000;
	case SYSPROP_DEVICE_TYPE:
#ifdef __SYMBIAN32__
		return DEVICE_TYPE_MOBILE;
#elif defined(BLACKBERRY)
		return DEVICE_TYPE_MOBILE;
#elif defined(MAEMO)
		return DEVICE_TYPE_MOBILE;
#elif defined(ANDROID)
		return DEVICE_TYPE_MOBILE;
#elif defined(Q_OS_LINUX)
		return DEVICE_TYPE_DESKTOP;
#elif defined(_WIN32)
		return DEVICE_TYPE_DESKTOP;
#elif defined(Q_OS_MAC)
		return DEVICE_TYPE_DESKTOP;
#else
		return DEVICE_TYPE_DESKTOP;
#endif
  default:
    return -1;
  }
}

void System_SendMessage(const char *command, const char *parameter) {
	if (!strcmp(command, "finish")) {
		qApp->exit(0);
	}
}

bool System_InputBoxGetString(const char *title, const char *defaultValue, char *outValue, size_t outLength)
{
	QString text = emugl->InputBoxGetQString(QString(title), QString(defaultValue));
	if (text.isEmpty())
		return false;
	strcpy(outValue, text.toStdString().c_str());
	return true;
}

void Vibrate(int length_ms) {
	if (length_ms == -1 || length_ms == -3)
		length_ms = 50;
	else if (length_ms == -2)
		length_ms = 25;
	// Symbian only for now
#if defined(__SYMBIAN32__)
	QFeedbackHapticsEffect effect;
	effect.setIntensity(0.8);
	effect.setDuration(length_ms);
	effect.start();
#endif
}

void LaunchBrowser(const char *url)
{
	QDesktopServices::openUrl(QUrl(url));
}

float CalculateDPIScale()
{
	// Sane default rather than check DPI
#ifdef __SYMBIAN32__
	return 1.4f;
#elif defined(USING_GLES2)
	return 1.2f;
#else
	return 1.0f;
#endif
}

static int mainInternal(QApplication &a)
{
#ifdef MOBILE_DEVICE
	emugl = new MainUI();
	emugl->resize(pixel_xres, pixel_yres);
	emugl->showFullScreen();
#endif
	EnableFZ();
	// Disable screensaver
#ifdef __SYMBIAN32__
	QSystemScreenSaver ssObject(emugl);
	ssObject.setScreenSaverInhibit();
	QScopedPointer<SymbianMediaKeys> mediakeys(new SymbianMediaKeys());
#elif defined(QT_HAS_SYSTEMINFO)
	QScreenSaver ssObject(emugl);
	ssObject.setScreenSaverEnabled(false);
#endif

#ifdef SDL
	SDLJoystick joy(true);
	joy.startEventLoop();
	SDL_Init(SDL_INIT_AUDIO);
	SDL_AudioSpec fmt, ret_fmt;
	memset(&fmt, 0, sizeof(fmt));
	fmt.freq = 44100;
	fmt.format = AUDIO_S16;
	fmt.channels = 2;
	fmt.samples = 2048;
	fmt.callback = &mixaudio;
	fmt.userdata = (void *)0;

	if (SDL_OpenAudio(&fmt, &ret_fmt) < 0) {
		ELOG("Failed to open audio: %s", SDL_GetError());
	} else {
		if (ret_fmt.samples != fmt.samples) // Notify, but still use it
			ELOG("Output audio samples: %d (requested: %d)", ret_fmt.samples, fmt.samples);
		if (ret_fmt.freq != fmt.freq || ret_fmt.format != fmt.format || ret_fmt.channels != fmt.channels) {
			ELOG("Sound buffer format does not match requested format.");
			ELOG("Output audio freq: %d (requested: %d)", ret_fmt.freq, fmt.freq);
			ELOG("Output audio format: %d (requested: %d)", ret_fmt.format, fmt.format);
			ELOG("Output audio channels: %d (requested: %d)", ret_fmt.channels, fmt.channels);
			ELOG("Provided output format does not match requirement, turning audio off");
			SDL_CloseAudio();
		}
	}
	SDL_PauseAudio(0);
#else
	QScopedPointer<MainAudio> audio(new MainAudio());
	audio->run();
#endif
	return a.exec();
}

#ifndef SDL
Q_DECL_EXPORT
#endif
int main(int argc, char *argv[])
{
#if defined(Q_OS_LINUX) && !defined(MAEMO)
	QApplication::setAttribute(Qt::AA_X11InitThreads, true);
#endif
	QApplication a(argc, argv);
	QSize res = QApplication::desktop()->screenGeometry().size();
	if (res.width() < res.height())
		res.transpose();
	pixel_xres = res.width();
	pixel_yres = res.height();
	g_dpi_scale = CalculateDPIScale();
	dp_xres = (int)(pixel_xres * g_dpi_scale); dp_yres = (int)(pixel_yres * g_dpi_scale);
	net::Init();
	std::string savegame_dir = ".";
	std::string assets_dir = ".";
#if QT_VERSION > QT_VERSION_CHECK(5, 0, 0)
	savegame_dir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation).toStdString();
	assets_dir = QStandardPaths::writableLocation(QStandardPaths::DataLocation).toStdString();
#elif defined(__SYMBIAN32__)
	savegame_dir = "E:/PPSSPP";
	assets_dir = "E:/PPSSPP";
#elif defined(BLACKBERRY)
	savegame_dir = "/accounts/1000/shared/misc";
	assets_dir = "app/native/assets";
#elif defined(MAEMO)
	savegame_dir = "/home/user/MyDocs/PPSSPP";
	assets_dir = "/opt/PPSSPP";
#endif
	savegame_dir += "/";
	assets_dir += "/";
	
	bool fullscreenCLI=false;
	for (int i = 1; i < argc; i++) 
	{
		if (!strcmp(argv[i],"--fullscreen"))
			fullscreenCLI=true;
	}
	NativeInit(argc, (const char **)argv, savegame_dir.c_str(), assets_dir.c_str(), "BADCOFFEE",fullscreenCLI);
	
	int ret = mainInternal(a);

#ifndef MOBILE_DEVICE
	exit(0);
#endif
	NativeShutdownGraphics();
#ifdef SDL
	SDL_PauseAudio(1);
	SDL_CloseAudio();
#endif
	NativeShutdown();
	net::Shutdown();
	return ret;
}

