#ifndef VBAM_WX_AUTOUPDATER_MACOS_SPARKLE_WRAPPER_H_
#define VBAM_WX_AUTOUPDATER_MACOS_SPARKLE_WRAPPER_H_

class SparkleWrapper
{
  public:
    void checkForUpdatesUi();
    void addAppcastURL(const char *appcastUrl);
    SparkleWrapper();
    ~SparkleWrapper();

  private:
    class Private;
    Private* d;
};

#endif // VBAM_WX_AUTOUPDATER_MACOS_SPARKLE_WRAPPER_H_
