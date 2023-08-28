#ifndef DSOINPUT_H
#define DSOINPUT_H

#include <QFile>
#include <QObject>
#include <QSettings>
#include <dsosettings.h>
#include <triggering.h>


struct SampleData
{
public:
    QString name = "";
    std::vector<double> data;
    float timeStamp = 0.0f;

    void addData(float time, float value, float frameRate);
    void addEmptyData(int num);
};

class DsoInput :public QObject
{
    Q_OBJECT
public:

public:
  /**
   * Creates a dsoControl object. The actual event loop / timer is not started.
   * You can optionally create a thread and move the created object to the
   * thread.
   * You need to call updateInterval() to start the timer. This is done implicitly
   * if run() is called.
   * @param device The usb device. This object does not take ownership.
   */
  explicit DsoInput(DsoSettings *settings, int verboseLevel );

  /// \brief Cleans up
  ~DsoInput() override;

  /// Call this to start the processing.
  /// This method will call itself periodically from there on.
  /// Move this class object to an own thread and call run from there.

  double getSamplerate() const { return controlsettings.samplerate.current; }

  unsigned getSamplesize() const {

  }

  /// \brief Stops the device.
  void quitSampling();

  void StartSample();

private:
  DsoSettings *dsoSettings = nullptr;
  SampleData* GetSampleData(const QString& name);
  QMap<QString, SampleData*> sampleDatas;
  QFile currFile;
  int cacheFilePosition;
  int ElapsedTimeMS = 0;

  bool bQuit = false;
  std::unique_ptr< Triggering > triggering;
  bool singleChannel = false;
  int verboseLevel = 0;
  void setSingleChannel( bool single ) { singleChannel = single; }
  bool isSingleChannel() const { return singleChannel; }
  bool triggerModeNONE() { return controlsettings.trigger.mode == Dso::TriggerMode::ROLL; }
  unsigned getRecordLength() const;
  void setDownsampling( unsigned downsampling ) { downsamplingNumber = downsampling; }

  void updateInterval();

  /// \brief Converts raw oscilloscope data to sample data
  void convertRawDataToSamples();

  /// \brief Restore the samplerate/timebase targets after divider updates.
  void restoreTargets();

  /// \brief Update the minimum and maximum supported samplerate.
  void updateSamplerateLimits();

  void controlSetSamplerate( uint8_t sampleIndex );
  Dso::ControlSettings controlsettings;           ///< The current settings of the device
  const DsoSettingsScope *scope = nullptr;        ///< Global scope parameters and configuations

  // Results
  unsigned downsamplingNumber = 1; ///< Number of downsamples to reduce sample rate
  DSOsamples result;
  unsigned expectedSampleCount = 0; ///< The expected total number of samples at
                                    /// the last check before sampling started
  bool calibrationHasChanged = false;
  std::unique_ptr< QSettings > calibrationSettings;
  double offsetCorrection[ HANTEK_GAIN_STEPS ][ HANTEK_CHANNEL_NUMBER ];
  double gainCorrection[ HANTEK_GAIN_STEPS ][ HANTEK_CHANNEL_NUMBER ];
  bool capturing = true;
  bool samplingStarted = false;
  bool stateMachineRunning = false;
  int acquireInterval = 3;
  int displayInterval = 0;
  unsigned activeChannels = 2;
  bool refresh = false; // parameter changed -> new raw to result conversion and trigger search needed
  void requestRefresh( bool active = true ) { refresh = active; }
  bool refreshNeeded() {
      bool changed = refresh;
      refresh = false;
      return changed;
  }

  unsigned debugLevel = 0;

#define dprintf( level, fmt, ... )               \
  do {                                         \
      if ( debugLevel & level )                \
          fprintf( stderr, fmt, __VA_ARGS__ ); \
  } while ( 0 )

public slots:
  /// \brief If sampling is disabled, no samplesAvailable() signals are send anymore, no samples
  /// are fetched from the device and no processing takes place.
  /// \param enabled Enables/Disables sampling
  void enableSamplingUI( bool enabled = true );

  /// \brief Sets the samplerate of the oscilloscope.
  /// \param samplerate The samplerate that should be met (S/s), 0.0 to restore
  /// current samplerate.
  /// \return The samplerate that has been set, 0.0 on error.
  Dso::ErrorCode setSamplerate( double samplerate = 0.0 );

  /// \brief Sets the time duration of one aquisition by adapting the samplerate.
  /// \param duration The record time duration that should be met (s), 0.0 to
  /// restore current record time.
  /// \return The record time duration that has been set, 0.0 on error.
  Dso::ErrorCode setRecordTime( double duration = 0.0 );

  /// \brief Enables/disables filtering of the given channel.
  /// \param channel The channel that should be set.
  /// \param used true if the channel should be sampled.
  /// \return See ::Dso::ErrorCode.
  Dso::ErrorCode setChannelUsed( ChannelID channel, bool used );

  /// \brief Enables/disables inverting of the given channel.
  /// \param channel The channel that should be set.
  /// \param used true if the channel is inverted.
  /// \return See ::Dso::ErrorCode.
  Dso::ErrorCode setChannelInverted( ChannelID channel, bool inverted );

  /// \brief Sets the probe gain for the given channel.
  /// \param channel The channel that should be set.
  /// \param probeAttn gain of probe is set.
  /// \return error code.
  Dso::ErrorCode setGain( ChannelID channel, double gain );

  /// \brief Set the trigger mode.
  /// \return See ::Dso::ErrorCode.
  Dso::ErrorCode setTriggerMode( Dso::TriggerMode mode );

  /// \brief Set the trigger source.
  /// \param id The channel that should be used as trigger.
  /// \return See ::Dso::ErrorCode.
  Dso::ErrorCode setTriggerSource( int channel );

  /// \brief Set the trigger smoothing.
  /// \param smooth The filter value.
  /// \return See ::Dso::ErrorCode.
  Dso::ErrorCode setTriggerSmooth( int smooth );

  /// \brief Set the trigger level.
  /// \param channel The channel that should be set.
  /// \param level The new trigger level (V).
  /// \return See ::Dso::ErrorCode.
  Dso::ErrorCode setTriggerLevel( ChannelID channel, double level );

  /// \brief Set the trigger slope.
  /// \param slope The Slope that should cause a trigger.
  /// \return See ::Dso::ErrorCode.
  Dso::ErrorCode setTriggerSlope( Dso::Slope slope );

  /// \brief Set the trigger position.
  /// \param position The new trigger position (in s).
  /// \return The trigger position that has been set.
  Dso::ErrorCode setTriggerPosition( double position );

  /// \brief Sets the calibration frequency of the oscilloscope.
  /// \param calfreq The calibration frequency.
  /// \return The tfrequency that has been set, ::Dso::ErrorCode on error.
  Dso::ErrorCode setCalFreq( double calfreq = 0.0 );

  /// \brief Initializes the device with the current settings.
  /// \param scope The settings for the oscilloscope.
  void applySettings( DsoSettingsScope *scope );

  /// \brief Starts a new sampling block.
  void restartSampling();

signals:
  void newChannelData(const DsoSettingsScope* scope);
  void newChannelData2();
  void showSamplingStatus( bool enabled );                   ///< The oscilloscope started/stopped sampling/waiting for trigger
  void statusMessage( const QString &message, int timeout ); ///< Status message about the oscilloscope
  void samplesAvailable( const DSOsamples *samples );        ///< New sample data is available
  void start();
  void samplerateChanged( double samplerate ); ///< The samplerate has changed

};

#endif // DSOINPUT_H
