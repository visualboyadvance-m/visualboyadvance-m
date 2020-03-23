#ifndef SPARKLE_WRAPPER_H
#define SPARKLE_WRAPPER_H

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

#endif // SPARKLE_WRAPPER_H
