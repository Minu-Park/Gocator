#include "QGocatorWidget.h"

#ifdef GOCATOR_HAS_UI
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QToolButton>
#include <QTreeWidget>
#include <QHeaderView>
#include <QScrollBar>
#include <QLabel>
#include <QStatusBar>
#include <QPointer>
#include <QMetaObject>
#include <QIcon>
#include <QSize>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QSignalBlocker>
#include <QDebug>
#include <QtConcurrent>
#include <cctype>

namespace
{
std::string formatDeviceName(const std::string& model, const std::string& serial, const std::string& address, bool isVirtual = false)
{
    std::string displayName = model;
    bool replaced = false;
    for (const std::string& target : {"Gocator ", "gocator ", "Gocator", "gocator"})
    {
        size_t pos = displayName.find(target);
        if (pos != std::string::npos)
        {
            displayName.replace(pos, target.length(), "G");
            replaced = true;
            break;
        }
    }
    if (!replaced && !displayName.empty() && std::isdigit(static_cast<unsigned char>(displayName[0])))
    {
        displayName = "G" + displayName;
    }
    
    std::string serialStr = serial;
    if (isVirtual)
    {
        serialStr = "Virtual";
    }
    else if (serialStr.empty() || serialStr == "0")
    {
        serialStr = "0";
    }
    
    return displayName + " (" + serialStr + ") - " + address;
}
}

QGocatorWidget::QGocatorWidget(QWidget *parent, Gocator *gocator)
    : QWidget(parent)
    , _gocator(gocator)
{
    setWindowTitle(QStringLiteral("LMI Gocator Control"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // IP selection & Control tools (Same layout style as QCameraWidget)
    _ipCombo = new QComboBox(this);
    _ipCombo->setObjectName(QStringLiteral("gocatorIpCombo"));
    _ipCombo->setEditable(true);
    _ipCombo->setMinimumWidth(120);
    _ipCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    _toolRefresh = new QToolButton(this);
    _toolRefresh->setObjectName(QStringLiteral("gocatorToolRefresh"));
    _toolRefresh->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-refresh-48.png")));
    _toolRefresh->setToolButtonStyle(Qt::ToolButtonIconOnly);
    _toolRefresh->setIconSize(QSize(16, 16));

    _toolConnect = new QToolButton(this);
    _toolConnect->setObjectName(QStringLiteral("gocatorToolConnect"));
    _toolConnect->setCheckable(true);
    _toolConnect->setToolButtonStyle(Qt::ToolButtonIconOnly);
    _toolConnect->setIconSize(QSize(16, 16));
    {
        QIcon icon;
        icon.addFile(QStringLiteral(":/Resources/Icons/icons8-connect-48.png"), QSize(), QIcon::Normal, QIcon::Off);
        icon.addFile(QStringLiteral(":/Resources/Icons/icons8-disconnected-48.png"), QSize(), QIcon::Normal, QIcon::On);
        _toolConnect->setIcon(icon);
    }

    _toolGrabOne = new QToolButton(this);
    _toolGrabOne->setObjectName(QStringLiteral("gocatorToolGrabOne"));
    _toolGrabOne->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-camera-48.png")));
    _toolGrabOne->setEnabled(false);
    _toolGrabOne->setToolButtonStyle(Qt::ToolButtonIconOnly);
    _toolGrabOne->setIconSize(QSize(16, 16));

    _toolGrabLive = new QToolButton(this);
    _toolGrabLive->setObjectName(QStringLiteral("gocatorToolGrabLive"));
    _toolGrabLive->setCheckable(true);
    _toolGrabLive->setEnabled(false);
    _toolGrabLive->setToolButtonStyle(Qt::ToolButtonIconOnly);
    _toolGrabLive->setIconSize(QSize(16, 16));
    {
        QIcon icon;
        icon.addFile(QStringLiteral(":/Resources/Icons/icons8-cameras-48.png"), QSize(), QIcon::Normal, QIcon::Off);
        icon.addFile(QStringLiteral(":/Resources/Icons/icons8-pause-48.png"), QSize(), QIcon::Normal, QIcon::On);
        _toolGrabLive->setIcon(icon);
    }

    auto *ipLayout = new QHBoxLayout;
    ipLayout->setContentsMargins(0, 0, 0, 0);
    ipLayout->setSpacing(8);
    ipLayout->addWidget(_ipCombo);
    ipLayout->addWidget(_toolRefresh);

    auto *toolLayout = new QHBoxLayout;
    toolLayout->setContentsMargins(0, 0, 0, 0);
    toolLayout->setSpacing(6);
    toolLayout->addWidget(_toolConnect);
    toolLayout->addWidget(_toolGrabOne);
    toolLayout->addWidget(_toolGrabLive);

    auto *topBarLayout = new QHBoxLayout;
    topBarLayout->setContentsMargins(12, 12, 12, 12);
    topBarLayout->setSpacing(10);
    topBarLayout->addLayout(ipLayout);
    topBarLayout->addLayout(toolLayout);

    // Param Tree (Same widget style as QCameraWidget Features Tree)
    _featuresWidget = new QTreeWidget(this);
    _featuresWidget->setObjectName(QStringLiteral("GocatorFeaturesTree"));
    _featuresWidget->setHeaderLabels(QStringList() << QStringLiteral("Feature") << QStringLiteral("Value"));
    _featuresWidget->setRootIsDecorated(true);
    _featuresWidget->setAnimated(false);
    _featuresWidget->setAlternatingRowColors(true);
    _featuresWidget->setUniformRowHeights(false);
    _featuresWidget->setIndentation(18);
    _featuresWidget->header()->setStretchLastSection(true);
    _featuresWidget->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    _featuresWidget->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    _featuresWidget->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    _featuresWidget->header()->setMinimumSectionSize(60);
    _featuresWidget->header()->resizeSection(0, 200);

    auto *treeLayout = new QVBoxLayout;
    treeLayout->setContentsMargins(12, 0, 12, 12);
    treeLayout->setSpacing(8);
    treeLayout->addWidget(_featuresWidget);

    mainLayout->addLayout(topBarLayout);
    mainLayout->addLayout(treeLayout);

    // Status Bar (Same style as QCameraWidget StatusBar)
    _statusBar = new QStatusBar(this);
    _statusBar->setObjectName(QStringLiteral("GocatorStatusBar"));
    _statusBar->setContentsMargins(0, 0, 0, 0);

    _statusLabel = new QLabel(QStringLiteral("Disconnected"), _statusBar);
    _statusLabel->setStyleSheet(QStringLiteral("color: #a12622; font-weight: 600;"));
    _statusBar->addWidget(_statusLabel);

    mainLayout->addWidget(_statusBar);

    // Connections
    connect(_toolRefresh, &QToolButton::clicked, this, &QGocatorWidget::onRefreshClicked);
    connect(_toolConnect, &QToolButton::toggled, this, &QGocatorWidget::onConnectToggled);
    connect(_toolGrabOne, &QToolButton::clicked, this, &QGocatorWidget::onGrabOneClicked);
    connect(_toolGrabLive, &QToolButton::toggled, this, &QGocatorWidget::onGrabLiveToggled);

    // Async Watchers
    connect(&_connectWatcher, &QFutureWatcher<bool>::finished, this, [this]() {
        if (_shuttingDown) return;
        bool success = _connectWatcher.result();
        setConnectionOperationActive(false);
        if (success)
        {
            applyConnectionState(true);
        }
        else
        {
            setStatus(QStringLiteral("Connection Failed"));
            QSignalBlocker blocker(_toolConnect);
            _toolConnect->setChecked(false);
            applyConnectionState(false);
        }
    });

    connect(&_discoverWatcher, &QFutureWatcher<std::vector<Gocator::DeviceInfo>>::finished, this, [this]() {
        if (_shuttingDown) return;
        std::vector<Gocator::DeviceInfo> devices = _discoverWatcher.result();
        
        QString currentAddress = ipAddress();
        _ipCombo->clear();

        if (devices.empty())
        {
            setStatus(QStringLiteral("No devices found"));
            if (!currentAddress.isEmpty())
            {
                _ipCombo->setEditText(currentAddress);
            }
            else
            {
                _ipCombo->setEditText(QStringLiteral("192.168.1.10"));
            }
            _toolRefresh->setEnabled(true);
            _toolConnect->setEnabled(true);
            return;
        }

        for (const auto& device : devices)
        {
            QString text = QString::fromStdString(formatDeviceName(device.model, device.serial, device.address));
            _ipCombo->addItem(text, QString::fromStdString(device.address));
        }

        if (!currentAddress.isEmpty())
        {
            int index = _ipCombo->findData(currentAddress);
            if (index >= 0) _ipCombo->setCurrentIndex(index);
            else _ipCombo->setEditText(currentAddress);
        }

        setStatus(QStringLiteral("Discovered %1 devices").arg(devices.size()));
        _toolRefresh->setEnabled(true);
        _toolConnect->setEnabled(true);
    });

    connect(&_paramWatcher, &QFutureWatcher<void>::finished, this, [this]() {
        if (_shuttingDown) return;
        updateFeatureValues();
    });

    if (_gocator)
    {
        _statusCallbackId = _gocator->registerStatusCallback([guard = QPointer<QGocatorWidget>(this)](Gocator::Status status, bool on) {
            if (guard)
            {
                QMetaObject::invokeMethod(guard.data(), [guard, status, on]() {
                    if (guard)
                    {
                        guard->handleStatusChanged(status, on);
                    }
                }, Qt::QueuedConnection);
            }
        });

        // Sync initial status
        applyConnectionState(_gocator->isOpened());
        if (_gocator->isGrabbing())
        {
            setStatus(QStringLiteral("Running"));
            setRunningState(true);
        }
    }
}

QGocatorWidget::~QGocatorWidget()
{
    prepareForShutdown();
    clearFeatures();
}

void QGocatorWidget::prepareForShutdown()
{
    _shuttingDown = true;
    _connectWatcher.cancel();
    _connectWatcher.waitForFinished();
    _discoverWatcher.cancel();
    _discoverWatcher.waitForFinished();
    _paramWatcher.cancel();
    _paramWatcher.waitForFinished();

    if (_gocator && _statusCallbackId != 0)
    {
        _gocator->deregisterStatusCallback(_statusCallbackId);
        _statusCallbackId = 0;
    }
    _gocator = nullptr;
}

void QGocatorWidget::onRefreshClicked()
{
    if (!_gocator || _shuttingDown) return;

    _toolRefresh->setEnabled(false);
    _toolConnect->setEnabled(false);
    setStatus(QStringLiteral("Discovering"));

    auto future = QtConcurrent::run([this]() {
        if (!_gocator) return std::vector<Gocator::DeviceInfo>();
        return _gocator->discoverDevices();
    });
    _discoverWatcher.setFuture(future);
}

void QGocatorWidget::onConnectToggled(bool toggled)
{
    if (!_gocator || _shuttingDown) return;

    setConnectionOperationActive(true);
    if (toggled)
    {
        setStatus(QStringLiteral("Connecting"));
        std::string ip = ipAddress().toStdString();
        auto future = QtConcurrent::run([this, ip]() {
            if (!_gocator) return false;
            return _gocator->open(ip);
        });
        _connectWatcher.setFuture(future);
    }
    else
    {
        auto future = QtConcurrent::run([this]() {
            if (_gocator) _gocator->close();
            return false;
        });
        _connectWatcher.setFuture(future);
    }
}

void QGocatorWidget::onGrabOneClicked()
{
    if (!_gocator || _shuttingDown) return;
    setStatus(QStringLiteral("Starting"));
    _gocator->configure(scanLengthMm(), scanMode(), intensityEnabled(), uniformSpacingEnabled(), exposureUs());
    _gocator->grab(1);
}

void QGocatorWidget::onGrabLiveToggled(bool toggled)
{
    if (!_gocator || _shuttingDown) return;

    if (toggled)
    {
        setStatus(QStringLiteral("Starting"));
        _gocator->configure(scanLengthMm(), scanMode(), intensityEnabled(), uniformSpacingEnabled(), exposureUs());
        _gocator->grab();
    }
    else
    {
        _gocator->stop();
    }
}

void QGocatorWidget::handleStatusChanged(Gocator::Status status, bool on)
{
    if (_shuttingDown) return;

    if (status == Gocator::GrabbingStatus)
    {
        setStatus(on ? QStringLiteral("Running") : QStringLiteral("Connected"));
        setRunningState(on);

        QSignalBlocker blocker(_toolGrabLive);
        _toolGrabLive->setChecked(on);
    }
    else if (status == Gocator::ConnectionStatus)
    {
        applyConnectionState(on);
        QSignalBlocker blocker(_toolConnect);
        _toolConnect->setChecked(on);
    }
}

void QGocatorWidget::setConnectionOperationActive(bool active)
{
    if (active)
    {
        _ipCombo->setEnabled(false);
        _toolRefresh->setEnabled(false);
        _toolConnect->setEnabled(false);
        return;
    }

    const bool opened = _gocator && _gocator->isOpened();
    _ipCombo->setEnabled(!opened);
    _toolRefresh->setEnabled(!opened);
    _toolConnect->setEnabled(true);
}

void QGocatorWidget::applyConnectionState(bool opened)
{
    if (_shuttingDown) return;

    _ipCombo->setEnabled(!opened);
    _toolRefresh->setEnabled(!opened);
    _toolGrabOne->setEnabled(opened);
    _toolGrabLive->setEnabled(opened);

    if (opened)
    {
        setStatus(QStringLiteral("Connected"));
        setRunningState(false);
        populateFeatures();

        if (_gocator)
        {
            Gocator::DeviceInfo devInfo = _gocator->getConnectedDeviceInfo();
            if (!devInfo.address.empty() && !devInfo.model.empty())
            {
                QString ip = QString::fromStdString(devInfo.address);
                QString text = QString::fromStdString(formatDeviceName(devInfo.model, devInfo.serial, devInfo.address, devInfo.isVirtual));

                int index = _ipCombo->findData(ip);
                if (index >= 0)
                {
                    _ipCombo->setItemText(index, text);
                    _ipCombo->setCurrentIndex(index);
                }
                else
                {
                    _ipCombo->addItem(text, ip);
                    int newIndex = _ipCombo->findData(ip);
                    if (newIndex >= 0) _ipCombo->setCurrentIndex(newIndex);
                }
            }
        }
    }
    else
    {
        {
            QSignalBlocker blocker(_toolGrabLive);
            _toolGrabLive->setChecked(false);
        }
        setStatus(QStringLiteral("Disconnected"));
        setRunningState(false);
        clearFeatures();
    }
}

void QGocatorWidget::setStatus(const QString& status)
{
    _statusLabel->setText(status);
    if (status == QStringLiteral("Running") || status == QStringLiteral("Connected") || status == QStringLiteral("Discovery completed"))
    {
        _statusLabel->setStyleSheet(QStringLiteral("color: #0a7a2f; font-weight: 600;"));
    }
    else if (status == QStringLiteral("Starting") || status == QStringLiteral("Connecting") || status == QStringLiteral("Discovering"))
    {
        _statusLabel->setStyleSheet(QStringLiteral("color: #b8860b; font-weight: 600;"));
    }
    else
    {
        _statusLabel->setStyleSheet(QStringLiteral("color: #a12622; font-weight: 600;"));
    }
}

void QGocatorWidget::setRunningState(bool running)
{
    _toolGrabOne->setEnabled(_gocator && _gocator->isOpened() && !running);
    _toolGrabLive->setEnabled(_gocator && _gocator->isOpened());

    for (auto it = _widgetToFeatureMap.begin(); it != _widgetToFeatureMap.end(); ++it)
    {
        if (it.key())
        {
            it.key()->setEnabled(!running);
        }
    }
}

QString QGocatorWidget::ipAddress() const
{
    QString currentData = _ipCombo->currentData().toString();
    if (currentData.isEmpty())
    {
        return _ipCombo->currentText().trimmed();
    }
    return currentData;
}

double QGocatorWidget::scanLengthMm() const
{
    QString path = QStringLiteral("/parameters/scanModeSettings/scanLengthMm");
    if (_pendingParams.contains(path)) return _pendingParams[path].toDouble();
    for (auto it = _widgetToFeatureMap.begin(); it != _widgetToFeatureMap.end(); ++it)
    {
        if (it.value().path == path)
        {
            if (auto* spin = qobject_cast<QDoubleSpinBox*>(it.key())) return spin->value();
        }
    }
    return 20.0;
}

Gocator::ScanMode QGocatorWidget::scanMode() const
{
    QString path = QStringLiteral("/parameters/scanModeSettings/scanMode");
    int val = 2; // Default Profile
    if (_pendingParams.contains(path)) val = _pendingParams[path].toInt();
    else
    {
        for (auto it = _widgetToFeatureMap.begin(); it != _widgetToFeatureMap.end(); ++it)
        {
            if (it.value().path == path)
            {
                if (auto* combo = qobject_cast<QComboBox*>(it.key())) val = combo->currentData().toInt();
            }
        }
    }
    return (val == 3) ? Gocator::ScanMode::SurfaceMode : Gocator::ScanMode::ProfileMode;
}

int QGocatorWidget::exposureUs() const
{
    QString path = QStringLiteral("/parameters/exposureSettings/singleExposure");
    if (_pendingParams.contains(path)) return _pendingParams[path].toInt();
    for (auto it = _widgetToFeatureMap.begin(); it != _widgetToFeatureMap.end(); ++it)
    {
        if (it.value().path == path)
        {
            if (auto* spin = qobject_cast<QSpinBox*>(it.key())) return spin->value();
        }
    }
    return 1000;
}

bool QGocatorWidget::intensityEnabled() const
{
    QString path = QStringLiteral("/parameters/scanModeSettings/intensityEnabled");
    if (_pendingParams.contains(path)) return _pendingParams[path].toBool();
    for (auto it = _widgetToFeatureMap.begin(); it != _widgetToFeatureMap.end(); ++it)
    {
        if (it.value().path == path)
        {
            if (auto* check = qobject_cast<QCheckBox*>(it.key())) return check->isChecked();
        }
    }
    return true;
}

bool QGocatorWidget::uniformSpacingEnabled() const
{
    QString path = QStringLiteral("/parameters/scanModeSettings/uniformSpacingEnabled");
    if (_pendingParams.contains(path)) return _pendingParams[path].toBool();
    for (auto it = _widgetToFeatureMap.begin(); it != _widgetToFeatureMap.end(); ++it)
    {
        if (it.value().path == path)
        {
            if (auto* check = qobject_cast<QCheckBox*>(it.key())) return check->isChecked();
        }
    }
    return true;
}

void QGocatorWidget::setIpAddress(const QString& ip)
{
    int index = _ipCombo->findData(ip);
    if (index >= 0)
    {
        _ipCombo->setCurrentIndex(index);
    }
    else
    {
        _ipCombo->setCurrentText(ip);
    }
}

void QGocatorWidget::setScanLengthMm(double length)
{
    QString path = QStringLiteral("/parameters/scanModeSettings/scanLengthMm");
    _pendingParams[path] = length;
    if (_gocator && _gocator->isOpened())
    {
        _gocator->setParameterValue("scanner", path.toStdString(), std::to_string(length));
        populateFeatures();
    }
}

void QGocatorWidget::setScanMode(Gocator::ScanMode mode)
{
    int val = (mode == Gocator::ScanMode::SurfaceMode) ? 3 : 2;
    QString path = QStringLiteral("/parameters/scanModeSettings/scanMode");
    _pendingParams[path] = val;
    if (_gocator && _gocator->isOpened())
    {
        _gocator->setParameterValue("scanner", path.toStdString(), std::to_string(val));
        populateFeatures();
    }
}

void QGocatorWidget::setExposureUs(int exposure)
{
    QString path = QStringLiteral("/parameters/exposureSettings/singleExposure");
    _pendingParams[path] = exposure;
    if (_gocator && _gocator->isOpened())
    {
        _gocator->setParameterValue("sensor", path.toStdString(), std::to_string(exposure));
        populateFeatures();
    }
}

void QGocatorWidget::setIntensityEnabled(bool enable)
{
    QString path = QStringLiteral("/parameters/scanModeSettings/intensityEnabled");
    _pendingParams[path] = enable;
    if (_gocator && _gocator->isOpened())
    {
        _gocator->setParameterValue("scanner", path.toStdString(), enable ? "true" : "false");
        populateFeatures();
    }
}

void QGocatorWidget::setUniformSpacingEnabled(bool enable)
{
    QString path = QStringLiteral("/parameters/scanModeSettings/uniformSpacingEnabled");
    _pendingParams[path] = enable;
    if (_gocator && _gocator->isOpened())
    {
        _gocator->setParameterValue("scanner", path.toStdString(), enable ? "true" : "false");
        populateFeatures();
    }
}

void QGocatorWidget::clearFeatures()
{
    _widgetToFeatureMap.clear();
    _featuresWidget->clear();
}

void QGocatorWidget::populateFeatures()
{
    if (!_gocator || _shuttingDown || !_gocator->isOpened()) return;

    _updatingFeatures = true;
    clearFeatures();

    QString scannerSchemaStr = QString::fromStdString(_gocator->getParametersSchema("scanner"));
    QString scannerDataStr = QString::fromStdString(_gocator->getParametersData("scanner"));
    QString sensorSchemaStr = QString::fromStdString(_gocator->getParametersSchema("sensor"));
    QString sensorDataStr = QString::fromStdString(_gocator->getParametersData("sensor"));

    QJsonObject scannerSchema = QJsonDocument::fromJson(scannerSchemaStr.toUtf8()).object();
    QJsonObject scannerData = QJsonDocument::fromJson(scannerDataStr.toUtf8()).object();
    QJsonObject sensorSchema = QJsonDocument::fromJson(sensorSchemaStr.toUtf8()).object();
    QJsonObject sensorData = QJsonDocument::fromJson(sensorDataStr.toUtf8()).object();

    QTreeWidgetItem* rootItem = new QTreeWidgetItem(_featuresWidget, QStringList() << QString::fromStdString(_gocator->getConnectedAddress()));
    rootItem->setSizeHint(0, QSize(0, 22));
    rootItem->setSizeHint(1, QSize(0, 22));

    // scanner 리소스 전개
    {
        QTreeWidgetItem* scannerCategory = new QTreeWidgetItem(rootItem, QStringList() << QStringLiteral("Scanner"));
        scannerCategory->setSizeHint(0, QSize(0, 22));
        scannerCategory->setSizeHint(1, QSize(0, 22));

        QJsonObject parametersSchema = scannerSchema.value(QStringLiteral("properties")).toObject()
                                                   .value(QStringLiteral("parameters")).toObject();
        QJsonObject parametersValues = scannerData.value(QStringLiteral("parameters")).toObject();

        QJsonObject subProps = parametersSchema.value(QStringLiteral("properties")).toObject();
        for (auto it = subProps.begin(); it != subProps.end(); ++it)
        {
            addFeatureNode(scannerCategory, QStringLiteral("scanner"), QStringLiteral("/parameters"), it.key(), it.value().toObject(), parametersValues);
        }
        scannerCategory->setExpanded(false);
    }

    // sensor 리소스 전개
    {
        QTreeWidgetItem* sensorCategory = new QTreeWidgetItem(rootItem, QStringList() << QStringLiteral("Sensor"));
        sensorCategory->setSizeHint(0, QSize(0, 22));
        sensorCategory->setSizeHint(1, QSize(0, 22));

        QJsonObject parametersSchema = sensorSchema.value(QStringLiteral("properties")).toObject()
                                                   .value(QStringLiteral("parameters")).toObject();
        QJsonObject parametersValues = sensorData.value(QStringLiteral("parameters")).toObject();

        QJsonObject subProps = parametersSchema.value(QStringLiteral("properties")).toObject();
        for (auto it = subProps.begin(); it != subProps.end(); ++it)
        {
            addFeatureNode(sensorCategory, QStringLiteral("sensor"), QStringLiteral("/parameters"), it.key(), it.value().toObject(), parametersValues);
        }
        sensorCategory->setExpanded(false);
    }

    rootItem->setExpanded(true);
    _updatingFeatures = false;
}

void QGocatorWidget::addFeatureNode(QTreeWidgetItem* parentItem, const QString& type, const QString& basePath, const QString& name, const QJsonObject& propSchema, const QJsonObject& valuesObj)
{
    QString propType = propSchema.value(QStringLiteral("type")).toString();
    QString path = basePath + "/" + name;
    
    // Use 'label' or 'title' for user-friendly name if available, otherwise fallback to the key 'name'
    QString displayName = propSchema.value(QStringLiteral("label")).toString();
    if (displayName.isEmpty()) displayName = propSchema.value(QStringLiteral("title")).toString();
    if (displayName.isEmpty()) displayName = name;

    if (propType == QStringLiteral("object"))
    {
        QJsonObject subProperties = propSchema.value(QStringLiteral("properties")).toObject();
        if (subProperties.isEmpty()) return;

        QTreeWidgetItem* groupItem = nullptr;
        if (parentItem)
        {
            groupItem = new QTreeWidgetItem(parentItem, QStringList() << displayName);
        }
        else
        {
            groupItem = new QTreeWidgetItem(_featuresWidget, QStringList() << displayName);
        }
        groupItem->setSizeHint(0, QSize(0, 22));
        groupItem->setSizeHint(1, QSize(0, 22));

        QJsonObject subValues = valuesObj.value(name).toObject();
        for (auto it = subProperties.begin(); it != subProperties.end(); ++it)
        {
            addFeatureNode(groupItem, type, path, it.key(), it.value().toObject(), subValues);
        }
        groupItem->setExpanded(false);
    }
    else
    {
        QTreeWidgetItem* item = nullptr;
        if (parentItem)
        {
            item = new QTreeWidgetItem(parentItem, QStringList() << displayName);
        }
        else
        {
            item = new QTreeWidgetItem(_featuresWidget, QStringList() << displayName);
        }

        QWidget* editorWidget = nullptr;
        QJsonValue curVal = valuesObj.value(name);

        if (_pendingParams.contains(path))
        {
            curVal = _pendingParams.value(path);
        }

        if (propType == QStringLiteral("boolean"))
        {
            QCheckBox* check = new QCheckBox(_featuresWidget);
            check->setChecked(curVal.toBool());
            connect(check, &QCheckBox::stateChanged, this, &QGocatorWidget::onParameterChanged);
            editorWidget = check;
        }
        else if (propType == QStringLiteral("integer") || propType == QStringLiteral("number"))
        {
            if (propSchema.contains(QStringLiteral("enum")))
            {
                QComboBox* combo = new QComboBox(_featuresWidget);
                QJsonArray enumVals = propSchema.value(QStringLiteral("enum")).toArray();
                QJsonArray enumTexts = propSchema.value(QStringLiteral("enumText")).toArray();
                int activeIndex = 0;
                for (int i = 0; i < enumVals.size(); ++i)
                {
                    QJsonValue enumV = enumVals.at(i);
                    QString displayText = (i < enumTexts.size()) ? enumTexts.at(i).toString() : QString();
                    if (enumV.isDouble())
                    {
                        if (displayText.isEmpty()) displayText = QString::number(enumV.toDouble());
                        combo->addItem(displayText, enumV.toDouble());
                        if (curVal.toDouble() == enumV.toDouble()) activeIndex = i;
                    }
                    else
                    {
                        if (displayText.isEmpty()) displayText = enumV.toString();
                        combo->addItem(displayText, enumV.toString());
                        if (curVal.toString() == enumV.toString()) activeIndex = i;
                    }
                }
                combo->setCurrentIndex(activeIndex);
                connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QGocatorWidget::onParameterChanged);
                editorWidget = combo;
            }
            else
            {
                double minVal = propSchema.value(QStringLiteral("minimum")).toDouble(0.0);
                double maxVal = propSchema.value(QStringLiteral("maximum")).toDouble(100000.0);

                if (propType == QStringLiteral("integer"))
                {
                    QSpinBox* spin = new QSpinBox(_featuresWidget);
                    spin->setRange(static_cast<int>(minVal), static_cast<int>(maxVal));
                    spin->setValue(curVal.toInt(0));
                    connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &QGocatorWidget::onParameterChanged);
                    editorWidget = spin;
                }
                else // number
                {
                    QDoubleSpinBox* spin = new QDoubleSpinBox(_featuresWidget);
                    spin->setRange(minVal, maxVal);
                    spin->setValue(curVal.toDouble(0.0));
                    spin->setSingleStep(0.1);
                    connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &QGocatorWidget::onParameterChanged);
                    editorWidget = spin;
                }
            }
        }
        else if (propType == QStringLiteral("string"))
        {
            if (propSchema.contains(QStringLiteral("enum")))
            {
                QComboBox* combo = new QComboBox(_featuresWidget);
                QJsonArray enumVals = propSchema.value(QStringLiteral("enum")).toArray();
                QJsonArray enumTexts = propSchema.value(QStringLiteral("enumText")).toArray();
                int activeIndex = 0;
                for (int i = 0; i < enumVals.size(); ++i)
                {
                    QString enumValue = enumVals.at(i).toString();
                    QString displayText = (i < enumTexts.size()) ? enumTexts.at(i).toString() : enumValue;
                    combo->addItem(displayText, enumValue);
                    if (curVal.toString() == enumValue) activeIndex = i;
                }
                combo->setCurrentIndex(activeIndex);
                connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QGocatorWidget::onParameterChanged);
                editorWidget = combo;
            }
        }

        if (editorWidget)
        {
            editorWidget->setObjectName(QStringLiteral("gocatorFeature_") + name);
            const int height = editorWidget->sizeHint().height();
            item->setSizeHint(0, QSize(0, height));
            item->setSizeHint(1, QSize(0, height));
            _featuresWidget->setItemWidget(item, 1, editorWidget);
            _widgetToFeatureMap.insert(editorWidget, FeatureMapping{type, path});
        }
    }
}

void QGocatorWidget::onParameterChanged()
{
    if (_updatingFeatures) return;

    QWidget* senderWidget = qobject_cast<QWidget*>(sender());
    if (!senderWidget || !_gocator || _shuttingDown) return;

    auto it = _widgetToFeatureMap.find(senderWidget);
    if (it == _widgetToFeatureMap.end()) return;

    const FeatureMapping& mapping = it.value();

    QJsonValue jsonVal;
    if (auto* check = qobject_cast<QCheckBox*>(senderWidget))
    {
        jsonVal = check->isChecked();
    }
    else if (auto* spin = qobject_cast<QSpinBox*>(senderWidget))
    {
        jsonVal = spin->value();
    }
    else if (auto* dspin = qobject_cast<QDoubleSpinBox*>(senderWidget))
    {
        jsonVal = dspin->value();
    }
    else if (auto* combo = qobject_cast<QComboBox*>(senderWidget))
    {
        jsonVal = QJsonValue::fromVariant(combo->currentData());
    }

    if (!jsonVal.isNull())
    {
        QString jsonValueStr;
        if (jsonVal.isBool())
        {
            jsonValueStr = jsonVal.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        }
        else if (jsonVal.isDouble())
        {
            jsonValueStr = QString::number(jsonVal.toDouble());
        }
        else if (jsonVal.isString())
        {
            jsonValueStr = "\"" + jsonVal.toString() + "\"";
        }

        std::string type = mapping.type.toStdString();
        std::string path = mapping.path.toStdString();
        std::string valStr = jsonValueStr.toStdString();

        auto future = QtConcurrent::run([this, type, path, valStr]() {
            if (_gocator)
            {
                _gocator->setParameterValue(type, path, valStr);
            }
        });
        _paramWatcher.setFuture(future);
    }
}

void QGocatorWidget::updateFeatureValues()
{
    if (!_gocator || _shuttingDown || !_gocator->isOpened()) return;

    struct DataResult {
        QString scannerData;
        QString sensorData;
    };

    auto future = QtConcurrent::run([this]() -> DataResult {
        if (!_gocator) return {};
        return {
            QString::fromStdString(_gocator->getParametersData("scanner")),
            QString::fromStdString(_gocator->getParametersData("sensor"))
        };
    });

    auto* watcher = new QFutureWatcher<DataResult>(this);
    connect(watcher, &QFutureWatcher<DataResult>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        if (_shuttingDown || !_gocator) return;
        
        DataResult res = watcher->result();
        
        QJsonObject scannerData = QJsonDocument::fromJson(res.scannerData.toUtf8()).object();
        QJsonObject sensorData = QJsonDocument::fromJson(res.sensorData.toUtf8()).object();

        QJsonObject scannerParams = scannerData.value(QStringLiteral("parameters")).toObject();
        QJsonObject sensorParams = sensorData.value(QStringLiteral("parameters")).toObject();

        _updatingFeatures = true;

        for (auto it = _widgetToFeatureMap.begin(); it != _widgetToFeatureMap.end(); ++it)
        {
            QWidget* widget = it.key();
            const FeatureMapping& mapping = it.value();

            if (!widget) continue;

            QString relativePath = mapping.path;
            if (relativePath.startsWith(QStringLiteral("/parameters")))
            {
                relativePath = relativePath.mid(11);
            }
            if (relativePath.startsWith(QStringLiteral("/")))
            {
                relativePath = relativePath.mid(1);
            }

            QStringList segments = relativePath.split(QStringLiteral("/"));
            QJsonObject currentObj = (mapping.type == QStringLiteral("scanner")) ? scannerParams : sensorParams;
            QJsonValue targetVal;

            for (int i = 0; i < segments.size(); ++i)
            {
                if (i == segments.size() - 1)
                {
                    targetVal = currentObj.value(segments[i]);
                }
                else
                {
                    currentObj = currentObj.value(segments[i]).toObject();
                }
            }

            if (targetVal.isUndefined() || targetVal.isNull())
            {
                continue;
            }

            QSignalBlocker blocker(widget);

            if (auto* check = qobject_cast<QCheckBox*>(widget))
            {
                check->setChecked(targetVal.toBool());
            }
            else if (auto* spin = qobject_cast<QSpinBox*>(widget))
            {
                spin->setValue(targetVal.toInt());
            }
            else if (auto* dspin = qobject_cast<QDoubleSpinBox*>(widget))
            {
                dspin->setValue(targetVal.toDouble());
            }
            else if (auto* combo = qobject_cast<QComboBox*>(widget))
            {
                int index = -1;
                if (targetVal.isDouble())
                {
                    index = combo->findData(targetVal.toDouble());
                }
                else
                {
                    index = combo->findData(targetVal.toString());
                }
                if (index >= 0)
                {
                    combo->setCurrentIndex(index);
                }
            }
        }

        _updatingFeatures = false;
    });
    watcher->setFuture(future);
}
#endif
