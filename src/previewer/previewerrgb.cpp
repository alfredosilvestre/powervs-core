#include "previewerrgb.h"

#include <QFile>
#include <GL/GLU.h>

extern "C"
{
    #include <libavutil/imgutils.h>
}

PreviewerRGB::PreviewerRGB(QWidget* parent) :
    QGLWidget(parent)
{
    filename = "bars/4-3.raw";
    videoWidth = 360;
    videoHeight = 288;
    frameSize = videoWidth * videoHeight * 4;
    half_videoWidth = videoWidth * 1.5;
    half_videoHeight = videoHeight * 1.5;
    aspect = videoWidth * 1.0 / videoHeight;
    eyez = videoHeight * 2.0 * aspect;
}

PreviewerRGB::~PreviewerRGB()
{
    glDeleteTextures(1, &texture);
}

void PreviewerRGB::initializeGL()
{
    // Define OpenGL initialization settings
    glFrontFace(GL_CW);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);

    glClearColor(0, 0, 0, 0);

    // Generate texture for display
    glGenTextures(1, &texture);

    glBindTexture(GL_TEXTURE_2D, texture);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, videoWidth, videoHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void PreviewerRGB::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    aspect = (GLfloat)w / (GLfloat)h;
    eyez = videoHeight * 2.0 * aspect;
    gluPerspective(45.0 - aspect, aspect, 1.0, eyez + 1.0);
}

void PreviewerRGB::resizeEvent(QResizeEvent* event)
{
    this->mutex.lock();
    this->makeCurrent();

    QGLWidget::resizeEvent(event);

    this->doneCurrent();
    this->mutex.unlock();
}

void PreviewerRGB::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    gluLookAt(0.0, 0.0, eyez,
              0.0, 0.0, 0.0,
              0.0, 1.0, 0.0);

    glColor3f(1.0f, 1.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_QUADS);
        glVertex2f(-half_videoWidth, -half_videoHeight); glTexCoord2i(0, 0);
        glVertex2f(-half_videoWidth, half_videoHeight); glTexCoord2i(1, 0);
        glVertex2f(half_videoWidth, half_videoHeight); glTexCoord2i(1, 1);
        glVertex2f(half_videoWidth, -half_videoHeight); glTexCoord2i(0, 1);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
}

void PreviewerRGB::paintEvent(QPaintEvent* event)
{
    this->mutex.lock();
    this->makeCurrent();

    QGLWidget::paintEvent(event);

    this->doneCurrent();
    this->mutex.unlock();
}

void PreviewerRGB::changeFormat(const QString& format)
{
    this->mutex.lock();
    this->makeCurrent();

    if(format == "PAL")
    {
        filename = "bars/4-3.raw";
        videoWidth = 360;
    }
    else if(format.contains("16:9"))
    {
        filename = "bars/16-9-SD.raw";
        videoWidth = 512;
    }
    else
    {
        filename = "bars/16-9-HD.raw";
        videoWidth = 512;
    }

    frameSize = videoWidth * videoHeight * 4;
    half_videoWidth = videoWidth * 1.5;

    glBindTexture(GL_TEXTURE_2D, texture);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, videoWidth, videoHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glBindTexture(GL_TEXTURE_2D, 0);

    this->doneCurrent();
    this->mutex.unlock();

    loadBars();
}

void PreviewerRGB::previewVideo(const QByteArray& videoBuffer)
{
    if(videoBuffer.size() != 0 && videoBuffer.size() == frameSize)
    {
        this->mutex.lock();
        this->makeCurrent();

        glBindTexture(GL_TEXTURE_2D, texture);

            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, videoWidth, videoHeight, GL_RGBA, GL_UNSIGNED_BYTE, videoBuffer.constData());

        glBindTexture(GL_TEXTURE_2D, 0);

        update();

        this->doneCurrent();
        this->mutex.unlock();
    }
}

void PreviewerRGB::previewAudio(const QByteArray& audioBuffer)
{
    emit setValues(audioBuffer);
}

void PreviewerRGB::loadBars()
{
    QFile file(filename);
    if(file.open(QIODevice::ReadOnly))
    {
        char* arr = new char[frameSize];

        QDataStream stream(&file);
        stream.readRawData(arr, frameSize);

        this->previewVideo(QByteArray(arr, frameSize));
        delete[] arr;
    }
}

void PreviewerRGB::stopAudio()
{
    emit setValues(QByteArray());
}
