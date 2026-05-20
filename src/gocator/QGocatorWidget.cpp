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
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QSignalBlocker>
#include <QDebug>

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
    _ipCombo->setStyleSheet(QStringLiteral(
        "min-height: 22px; max-height: 22px; padding: 0 6px; border: 1px solid #cfd9e4; border-radius: 8px; background: #ffffff; color: #16202b;"
    ));

    const QString toolButtonStyle = QStringLiteral(
        "min-width: 20px; min-height: 20px; max-width: 20px; max-height: 20px; padding: 0; border-radius: 8px;"
    );

    _toolRefresh = new QToolButton(this);
    _toolRefresh->setObjectName(QStringLiteral("gocatorToolRefresh"));
    _toolRefresh->setIcon(QIcon(QStringLiteral(":/Resources/Icons/icons8-refresh-48.png")));
    _toolRefresh->setToolButtonStyle(Qt::ToolButtonIconOnly);
    _toolRefresh->setIconSize(QSize(16, 16));
    _toolRefresh->setStyleSheet(toolButtonStyle);

    _toolConnect = new QToolButton(this);
    _toolConnect->setObjectName(QStringLiteral("gocatorToolConnect"));
    _toolConnect->setCheckable(true);
    _toolConnect->setToolButtonStyle(Qt::ToolButtonIconOnly);
    _toolConnect->setIconSize(QSize(16, 16));
    _toolConnect->setStyleSheet(toolButtonStyle);
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
    _toolGrabOne->setStyleSheet(toolButtonStyle);

    _toolGrabLive = new QToolButton(this);
    _toolGrabLive->setObjectName(QStringLiteral("gocatorToolGrabLive"));
    _toolGrabLive->setCheckable(true);
    _toolGrabLive->setEnabled(false);
    _toolGrabLive->setToolButtonStyle(Qt::ToolButtonIconOnly);
    _toolGrabLive->setIconSize(QSize(16, 16));
    _toolGrabLive->setStyleSheet(toolButtonStyle);
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
    _featuresWidget->setIndentation(12);
    _featuresWidget->header()->setStretchLastSection(true);
    _featuresWidget->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    _featuresWidget->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    _featuresWidget->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    _featuresWidget->header()->setMinimumSectionSize(60);
    _featuresWidget->header()->resizeSection(0, 150);

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

    setStatus(QStringLiteral("Discovering"));
    _ipCombo->clear();

    std::vector<Gocator::DeviceInfo> devices = _gocator->discoverDevices();
    if (devices.empty())
    {
        setStatus(QStringLiteral("No devices found"));
        return;
    }

    for (const auto& device : devices)
    {
        QString text = QString::fromStdString(device.address + " (" + device.model + " S/N:" + device.serial + ")");
        _ipCombo->addItem(text, QString::fromStdString(device.address));
    }

    setStatus(QStringLiteral("Discovery completed"));
}

void QGocatorWidget::onConnectToggled(bool toggled)
{
    if (!_gocator || _shuttingDown) return;

    setConnectionOperationActive(true);
    if (toggled)
    {
        setStatus(QStringLiteral("Connecting"));
        if (_gocator->open(ipAddress().toStdString()))
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
    }
    else
    {
        _gocator->close();
        applyConnectionState(false);
    }
    setConnectionOperationActive(false);
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

    clearFeatures();

    QString scannerSchemaStr = QString::fromStdString(_gocator->getParametersSchema("scanner"));
    QString scannerDataStr = QString::fromStdString(_gocator->getParametersData("scanner"));
    QString sensorSchemaStr = QString::fromStdString(_gocator->getParametersSchema("sensor"));
    QString sensorDataStr = QString::fromStdString(_gocator->getParametersData("sensor"));

    QJsonObject scannerSchema = QJsonDocument::fromJson(scannerSchemaStr.toUtf8()).object();
    QJsonObject scannerData = QJsonDocument::fromJson(scannerDataStr.toUtf8()).object();
    QJsonObject sensorSchema = QJsonDocument::fromJson(sensorSchemaStr.toUtf8()).object();
    QJsonObject sensorData = QJsonDocument::fromJson(sensorDataStr.toUtf8()).object();

    // scanner 리소스 전개
    {
        QJsonObject parametersSchema = scannerSchema.value(QStringLiteral("properties")).toObject()
                                                   .value(QStringLiteral("parameters")).toObject();
        QJsonObject parametersValues = scannerData.value(QStringLiteral("parameters")).toObject();

        QJsonObject subProps = parametersSchema.value(QStringLiteral("properties")).toObject();
        for (auto it = subProps.begin(); it != subProps.end(); ++it)
        {
            addFeatureNode(nullptr, QStringLiteral("scanner"), QStringLiteral("/parameters"), it.key(), it.value().toObject(), parametersValues);
        }
    }

    // sensor 리소스 전개
    {
        QJsonObject parametersSchema = sensorSchema.value(QStringLiteral("properties")).toObject()
                                                   .value(QStringLiteral("parameters")).toObject();
        QJsonObject parametersValues = sensorData.value(QStringLiteral("parameters")).toObject();

        QJsonObject subProps = parametersSchema.value(QStringLiteral("properties")).toObject();
        for (auto it = subProps.begin(); it != subProps.end(); ++it)
        {
            addFeatureNode(nullptr, QStringLiteral("sensor"), QStringLiteral("/parameters"), it.key(), it.value().toObject(), parametersValues);
        }
    }
}

void QGocatorWidget::addFeatureNode(QTreeWidgetItem* parentItem, const QString& type, const QString& basePath, const QString& name, const QJsonObject& propSchema, const QJsonObject& valuesObj)
{
    QString propType = propSchema.value(QStringLiteral("type")).toString();
    QString path = basePath + "/" + name;

    if (propType == QStringLiteral("object"))
    {
        QJsonObject subProperties = propSchema.value(QStringLiteral("properties")).toObject();
        if (subProperties.isEmpty()) return;

        QTreeWidgetItem* groupItem = nullptr;
        if (parentItem)
        {
            groupItem = new QTreeWidgetItem(parentItem, QStringList() << name);
        }
        else
        {
            groupItem = new QTreeWidgetItem(_featuresWidget, QStringList() << name);
        }

        QJsonObject subValues = valuesObj.value(name).toObject();
        for (auto it = subProperties.begin(); it != subProperties.end(); ++it)
        {
            addFeatureNode(groupItem, type, path, it.key(), it.value().toObject(), subValues);
        }
        groupItem->setExpanded(true);
    }
    else
    {
        QTreeWidgetItem* item = nullptr;
        if (parentItem)
        {
            item = new QTreeWidgetItem(parentItem, QStringList() << name);
        }
        else
        {
            item = new QTreeWidgetItem(_featuresWidget, QStringList() << name);
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
                int activeIndex = 0;
                for (int i = 0; i < enumVals.size(); ++i)
                {
                    QJsonValue enumV = enumVals.at(i);
                    QString enumText;
                    if (enumV.isDouble())
                    {
                        enumText = QString::number(enumV.toDouble());
                        combo->addItem(enumText, enumV.toDouble());
                        if (curVal.toDouble() == enumV.toDouble()) activeIndex = i;
                    }
                    else
                    {
                        enumText = enumV.toString();
                        combo->addItem(enumText, enumV.toString());
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
                int activeIndex = 0;
                for (int i = 0; i < enumVals.size(); ++i)
                {
                    QString enumText = enumVals.at(i).toString();
                    combo->addItem(enumText, enumText);
                    if (curVal.toString() == enumText) activeIndex = i;
                }
                combo->setCurrentIndex(activeIndex);
                connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QGocatorWidget::onParameterChanged);
                editorWidget = combo;
            }
        }

        if (editorWidget)
        {
            editorWidget->setObjectName(QStringLiteral("gocatorFeature_") + name);
            const int height = editorWidget->sizeHint().height() + 4;
            item->setSizeHint(0, QSize(0, height));
            item->setSizeHint(1, QSize(0, height));
            _featuresWidget->setItemWidget(item, 1, editorWidget);
            _widgetToFeatureMap.insert(editorWidget, FeatureMapping{type, path});
        }
    }
}

void QGocatorWidget::onParameterChanged()
{
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

        _gocator->setParameterValue(mapping.type.toStdString(), mapping.path.toStdString(), jsonValueStr.toStdString());
    }
}
#endif
