#ifndef PREVIEWERRGB_H
#define PREVIEWERRGB_H

#include <QGLWidget>
#include <QMutex>

class PreviewerRGB : public QGLWidget
{
    Q_OBJECT

    public:
        explicit PreviewerRGB(QWidget* parent = 0);
        ~PreviewerRGB();

    public:
        void loadBars();
        void changeFormat(const QString& format);

    protected:
        void initializeGL();
        void resizeGL(int w, int h);
        void resizeEvent(QResizeEvent* event);
        void paintGL();
        void paintEvent(QPaintEvent* paintEvent);

    // OpenGL variables
    private:
        GLuint texture;

    private:
        QString filename;
        int videoWidth;
        int videoHeight;
        int frameSize;
        float half_videoWidth;
        float half_videoHeight;
        float aspect;
        float eyez;
        QMutex mutex;

    signals:
        void setValues(const QByteArray& audioBuffer);

    public slots:
        void previewVideo(const QByteArray& videoBuffer);
        void previewAudio(const QByteArray& audioBuffer);
        void stopAudio();
};

#endif // PREVIEWERRGB_H
