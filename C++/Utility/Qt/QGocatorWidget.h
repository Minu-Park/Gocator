#ifndef QGOCATORWIDGET_H
#define QGOCATORWIDGET_H

#ifdef GOCATOR_HAS_UI
#include <QJsonValue>
#include <QMap>
#include <QWidget>
#include <QString>
#include <QFutureWatcher>
#include "Gocator.h"

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QToolButton;
class QTreeWidget;
class QLabel;
class QTimer;
class QStatusBar;

class QGocatorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit QGocatorWidget(QWidget *parent = nullptr, Gocator *gocator = nullptr);
    ~QGocatorWidget() override;

    void setStatus(const QString& status);
    void setRunningState(bool running);

    QString ipAddress() const;
    double scanLengthMm() const;
    Gocator::ScanMode scanMode() const;
    int exposureUs() const;
    bool intensityEnabled() const;
    bool uniformSpacingEnabled() const;

    void setIpAddress(const QString& ip);
    void setScanLengthMm(double length);
    void setScanMode(Gocator::ScanMode mode);
    void setExposureUs(int exposure);
    void setIntensityEnabled(bool enable);
    void setUniformSpacingEnabled(bool enable);
    void prepareForShutdown();

private slots:
    void onRefreshClicked();
    void onConnectToggled(bool toggled);
    void onGrabOneClicked();
    void onGrabLiveToggled(bool toggled);
    void handleStatusChanged(Gocator::Status status, bool on);
    void onParameterChanged();

private:
    struct FeatureMapping {
        Gocator::ParameterTarget target;
        QString path; // JSON pointer path
        QString label;
    };

    struct FeatureDataResult {
        QString scannerData;
        QString sensorData;
    };

    void setConnectionOperationActive(bool active);
    void applyConnectionState(bool opened);
    void populateFeatures();
    void clearFeatures();
    void addFeatureNode(class QTreeWidgetItem* parentItem, Gocator::ParameterTarget target, const QString& basePath, const QString& name, const class QJsonObject& propSchema, const class QJsonObject& valuesObj);
    void updateFeatureValues();
    void applyFeatureValues(const FeatureDataResult& result);

    Gocator *_gocator = nullptr;
    Gocator::CallbackId _statusCallbackId = 0;
    bool _shuttingDown = false;
    bool _connectionAttempted = false;

    QComboBox *_ipCombo = nullptr;
    QToolButton *_toolRefresh = nullptr;
    QToolButton *_toolConnect = nullptr;
    QToolButton *_toolGrabOne = nullptr;
    QToolButton *_toolGrabLive = nullptr;

    QTreeWidget *_featuresWidget = nullptr;
    QMap<QWidget*, FeatureMapping> _widgetToFeatureMap;
    QMap<QString, class QJsonValue> _pendingParams;

    QStatusBar *_statusBar = nullptr;
    QLabel *_messageLabel = nullptr;
    QLabel *_statusLabel = nullptr;
    QTimer *_messageTimer = nullptr;

    void showStatusMessage(const QString& msg, bool isError = false, int timeout = 0);
    void updateGrabState(bool grabbing);
    void updateStatusBubble();

    QFutureWatcher<bool> _connectWatcher;
    QFutureWatcher<std::vector<Gocator::DeviceInfo>> _discoverWatcher;
    QFutureWatcher<void> _paramWatcher;
    QFutureWatcher<FeatureDataResult> _featureDataWatcher;
    bool _updatingFeatures = false;
    bool _grabbing = false;
};
#endif

#endif // QGOCATORWIDGET_H
