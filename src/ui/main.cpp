#include <chrono>
#include <algorithm>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "gocator/GocatorAcquisition.h"
#include "gocator/GocatorDiscovery.h"
#include "gocator/GocatorSettingsManager.h"

namespace
{

constexpr const char* kDefaultManualAddress = "192.168.1.10";

struct OperationResult
{
    bool ok = false;
    std::string message;
    std::string selectedAddress;
    std::vector<gocator::GocatorDeviceInfo> devices;

    bool connectedKnown = false;
    bool connected = false;
    bool takeSettings = false;
    bool clearSettings = false;
    std::shared_ptr<gocator::GocatorSettingsManager> settings;

    bool hasScanner = false;
    gocator::ScannerInfo scanner;

    bool hasFrame = false;
    gocator::GocatorFrame frame;
};

QString toQString(const std::string& value)
{
    return QString::fromStdString(value);
}

void appendLog(QPlainTextEdit& log, const QString& text)
{
    const QString now = QDateTime::currentDateTime().toString("HH:mm:ss");
    log.appendPlainText(QString("[%1] %2").arg(now, text));
}

std::string scannerInfoText(const gocator::ScannerInfo& scanner)
{
    std::ostringstream out;
    out << "model=" << scanner.model << '\n'
        << "serial=" << scanner.serialNumber << '\n'
        << "engine=" << scanner.engineId << '\n'
        << "scannerId=" << scanner.scannerId << '\n'
        << "scannerPath=" << scanner.scannerPath << '\n'
        << "sensorPath=" << scanner.sensorPath << '\n'
        << "profileSource=" << scanner.profileSourceId;
    return out.str();
}

std::string frameText(const gocator::GocatorFrame& frame)
{
    std::ostringstream out;
    out << "messages=" << frame.messageCount << '\n'
        << "images=" << frame.images.size() << '\n'
        << "profiles=" << frame.profiles.size() << '\n'
        << "spots=" << frame.spots.size();

    for (const gocator::GocatorFrameMessage& message : frame.messages)
    {
        out << '\n'
            << message.typeName
            << " type=" << message.typeValue
            << " source=" << message.sourceId
            << " dataSet=" << message.dataSetId
            << " gdpId=" << message.gdpId
            << " last=" << (message.isLastMessage ? "yes" : "no");
    }

    for (const gocator::GocatorImageFrame& image : frame.images)
    {
        out << '\n'
            << "image "
            << image.width << "x" << image.height
            << " pixelSize=" << image.pixelSize
            << " format=" << image.pixelFormatName
            << " bytes=" << image.dataSize
            << " source=" << image.sourceId;
        if (image.hasByteStats)
        {
            out << " byte[min,max,mean]="
                << static_cast<int>(image.minByte) << ","
                << static_cast<int>(image.maxByte) << ","
                << image.meanByte;
        }
    }

    for (const gocator::GocatorProfileFrame& profile : frame.profiles)
    {
        out << '\n'
            << "profile "
            << "points=" << profile.width
            << " valid=" << profile.validCount
            << " null=" << profile.nullCount
            << " xRes=" << profile.xResolution
            << " zRes=" << profile.zResolution
            << " source=" << profile.sourceId;

        if (profile.hasRangeStats)
        {
            out << " range[first,min,max]="
                << profile.firstRange << ","
                << profile.minRange << ","
                << profile.maxRange;
        }

        if (profile.hasIntensityStats)
        {
            out << " intensity[min,max]="
                << profile.minIntensity << ","
                << profile.maxIntensity;
        }
    }

    for (const gocator::GocatorSpotsFrame& spots : frame.spots)
    {
        out << '\n'
            << "spots "
            << "points=" << spots.pointCount
            << " exposure=" << spots.exposure
            << " columnBased=" << spots.columnBased
            << " center[min,max]=" << spots.spotCenterMin << ","
            << spots.spotCenterMax
            << " source=" << spots.sourceId;
    }

    return out.str();
}

void setDevices(QTableWidget& table, const std::vector<gocator::GocatorDeviceInfo>& devices)
{
    table.setRowCount(static_cast<int>(devices.size()));
    for (int row = 0; row < static_cast<int>(devices.size()); ++row)
    {
        const gocator::GocatorDeviceInfo& device = devices[static_cast<std::size_t>(row)];
        table.setItem(row, 0, new QTableWidgetItem(toQString(device.address)));
        table.setItem(row, 1, new QTableWidgetItem(toQString(device.deviceModel)));
        table.setItem(row, 2, new QTableWidgetItem(QString::number(device.serialNumber)));
        table.setItem(row, 3, new QTableWidgetItem(QString::number(device.controlPort)));
        table.setItem(row, 4, new QTableWidgetItem(device.canConnectLocally() ? "yes" : "no"));
    }
}

gocator::GocatorConnectionConfig configFromUi(const QLineEdit& address, const QSpinBox& port, const QSpinBox& timeout)
{
    return gocator::GocatorDiscovery::manualTarget(
        address.text().trimmed().toStdString(),
        static_cast<std::uint16_t>(port.value()),
        timeout.value());
}

QImage imagePreview(const gocator::GocatorImageFrame& frame)
{
    if (frame.width == 0 || frame.height == 0 || frame.pixels.empty())
    {
        return {};
    }

    const int width = static_cast<int>(frame.width);
    const int height = static_cast<int>(frame.height);
    const int pixelSize = static_cast<int>(frame.pixelSize);
    const std::uint8_t* data = frame.pixels.data();

    if (pixelSize == 1 && frame.pixels.size() >= static_cast<std::size_t>(width * height))
    {
        return QImage(data, width, height, width, QImage::Format_Grayscale8).copy();
    }

    if (pixelSize == 2 && frame.pixels.size() >= static_cast<std::size_t>(width * height * pixelSize))
    {
        QImage image(width, height, QImage::Format_Grayscale8);
        for (int y = 0; y < height; ++y)
        {
            auto* target = image.scanLine(y);
            const auto* source = data + static_cast<std::size_t>(y * width * pixelSize);
            for (int x = 0; x < width; ++x)
            {
                target[x] = source[x * 2 + 1];
            }
        }
        return image;
    }

    if (pixelSize == 3 && frame.pixels.size() >= static_cast<std::size_t>(width * height * pixelSize))
    {
        QImage image(data, width, height, width * pixelSize, QImage::Format_RGB888);
        if (frame.pixelFormatName.find("BGR") != std::string::npos || frame.pixelFormatName.find("Bgr") != std::string::npos)
        {
            return image.rgbSwapped().copy();
        }
        return image.copy();
    }

    return {};
}

QImage profilePreview(const gocator::GocatorProfileFrame& profile, QSize size)
{
    const int width = std::max(320, size.width());
    const int height = std::max(220, size.height());
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(QColor(24, 24, 24));

    if (profile.validCount == 0)
    {
        return image;
    }

    double minX = 0.0;
    double maxX = 0.0;
    double minZ = 0.0;
    double maxZ = 0.0;
    bool first = true;

    for (const gocator::GocatorProfilePoint& point : profile.points)
    {
        if (!point.valid)
        {
            continue;
        }

        if (first)
        {
            minX = maxX = point.x;
            minZ = maxZ = point.z;
            first = false;
        }
        else
        {
            minX = std::min(minX, point.x);
            maxX = std::max(maxX, point.x);
            minZ = std::min(minZ, point.z);
            maxZ = std::max(maxZ, point.z);
        }
    }

    if (maxX <= minX)
    {
        maxX = minX + 1.0;
    }
    if (maxZ <= minZ)
    {
        maxZ = minZ + 1.0;
    }

    constexpr int margin = 20;
    const double plotWidth = static_cast<double>(width - margin * 2);
    const double plotHeight = static_cast<double>(height - margin * 2);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(80, 80, 80), 1));
    painter.drawRect(margin, margin, width - margin * 2, height - margin * 2);

    painter.setPen(QPen(QColor(0, 210, 170), 2));
    QPoint previous;
    bool hasPrevious = false;

    for (const gocator::GocatorProfilePoint& point : profile.points)
    {
        if (!point.valid)
        {
            hasPrevious = false;
            continue;
        }

        const int x = margin + static_cast<int>((point.x - minX) / (maxX - minX) * plotWidth);
        const int y = height - margin - static_cast<int>((point.z - minZ) / (maxZ - minZ) * plotHeight);
        const QPoint current(x, y);
        if (hasPrevious)
        {
            painter.drawLine(previous, current);
        }
        previous = current;
        hasPrevious = true;
    }

    painter.setPen(QColor(220, 220, 220));
    painter.drawText(12, 16, QString("valid %1 / %2").arg(profile.validCount).arg(profile.width));
    return image;
}

void setStatus(QLabel& state, QLabel& endpoint, QLabel& gdp, bool connected, const QString& endpointText)
{
    state.setText(connected ? "Connected" : "Disconnected");
    state.setStyleSheet(connected ? "color: #0a7a2f; font-weight: 600;" : "color: #a12622; font-weight: 600;");
    endpoint.setText(endpointText.isEmpty() ? "-" : endpointText);
    if (!connected)
    {
        gdp.setText("-");
    }
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("Gocator Debug UI");
    window.resize(1180, 820);

    auto* central = new QWidget(&window);
    auto* root = new QVBoxLayout(central);

    auto* connectionBox = new QGroupBox("Connection", central);
    auto* connectionLayout = new QHBoxLayout(connectionBox);

    auto* addressEdit = new QLineEdit(kDefaultManualAddress, connectionBox);
    auto* portSpin = new QSpinBox(connectionBox);
    portSpin->setRange(1, 65535);
    portSpin->setValue(gocator::kDefaultControlPort);

    auto* timeoutSpin = new QSpinBox(connectionBox);
    timeoutSpin->setRange(100, 120000);
    timeoutSpin->setValue(gocator::kDefaultCommandTimeoutMs);
    timeoutSpin->setSingleStep(1000);

    auto* connectButton = new QPushButton("Connect", connectionBox);
    auto* disconnectButton = new QPushButton("Disconnect", connectionBox);
    auto* discoverButton = new QPushButton("Discover", connectionBox);

    connectionLayout->addWidget(new QLabel("IP", connectionBox));
    connectionLayout->addWidget(addressEdit, 3);
    connectionLayout->addWidget(new QLabel("Control", connectionBox));
    connectionLayout->addWidget(portSpin, 1);
    connectionLayout->addWidget(new QLabel("Timeout", connectionBox));
    connectionLayout->addWidget(timeoutSpin, 1);
    connectionLayout->addWidget(connectButton);
    connectionLayout->addWidget(disconnectButton);
    connectionLayout->addWidget(discoverButton);

    auto* statusBox = new QGroupBox("Status", central);
    auto* statusLayout = new QFormLayout(statusBox);
    auto* statusValue = new QLabel("Disconnected", statusBox);
    auto* endpointValue = new QLabel("-", statusBox);
    auto* gdpValue = new QLabel("-", statusBox);
    auto* frameValue = new QLabel("-", statusBox);
    statusLayout->addRow("Connection", statusValue);
    statusLayout->addRow("Endpoint", endpointValue);
    statusLayout->addRow("GDP", gdpValue);
    statusLayout->addRow("Last frame", frameValue);

    auto* infoBox = new QGroupBox("Scanner Info", central);
    auto* infoLayout = new QVBoxLayout(infoBox);
    auto* infoButton = new QPushButton("Refresh Info", infoBox);
    auto* scannerInfoEdit = new QPlainTextEdit(infoBox);
    scannerInfoEdit->setReadOnly(true);
    scannerInfoEdit->setMaximumHeight(130);
    infoLayout->addWidget(infoButton);
    infoLayout->addWidget(scannerInfoEdit);

    auto* topRow = new QWidget(central);
    auto* topLayout = new QHBoxLayout(topRow);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->addWidget(statusBox, 1);
    topLayout->addWidget(infoBox, 2);

    auto* scanSettingsBox = new QGroupBox("Scan Settings", central);
    auto* scanSettingsLayout = new QHBoxLayout(scanSettingsBox);
    auto* scanModeSpin = new QSpinBox(scanSettingsBox);
    scanModeSpin->setRange(0, 10);
    scanModeSpin->setValue(2);
    auto* intensityCheck = new QCheckBox("Intensity", scanSettingsBox);
    intensityCheck->setChecked(true);
    auto* uniformSpacingCheck = new QCheckBox("Uniform spacing", scanSettingsBox);
    uniformSpacingCheck->setChecked(true);
    auto* exposureCheck = new QCheckBox("Exposure", scanSettingsBox);
    auto* exposureSpin = new QSpinBox(scanSettingsBox);
    exposureSpin->setRange(1, 1000000);
    exposureSpin->setValue(1000);
    exposureSpin->setSingleStep(100);
    auto* applyScanSettingsButton = new QPushButton("Apply", scanSettingsBox);
    auto* readScannerButton = new QPushButton("Read Scanner", scanSettingsBox);
    auto* schemaScannerButton = new QPushButton("Schema", scanSettingsBox);

    scanSettingsLayout->addWidget(new QLabel("Scan mode", scanSettingsBox));
    scanSettingsLayout->addWidget(scanModeSpin);
    scanSettingsLayout->addWidget(intensityCheck);
    scanSettingsLayout->addWidget(uniformSpacingCheck);
    scanSettingsLayout->addWidget(exposureCheck);
    scanSettingsLayout->addWidget(exposureSpin);
    scanSettingsLayout->addStretch(1);
    scanSettingsLayout->addWidget(applyScanSettingsButton);
    scanSettingsLayout->addWidget(readScannerButton);
    scanSettingsLayout->addWidget(schemaScannerButton);

    auto* acquisitionBox = new QGroupBox("Acquisition", central);
    auto* acquisitionLayout = new QVBoxLayout(acquisitionBox);
    auto* acquisitionControls = new QWidget(acquisitionBox);
    auto* acquisitionControlsLayout = new QHBoxLayout(acquisitionControls);
    acquisitionControlsLayout->setContentsMargins(0, 0, 0, 0);

    auto* outputSourceEdit = new QLineEdit(acquisitionBox);
    outputSourceEdit->setPlaceholderText("GDP output source");
    auto* receiveTimeoutSpin = new QSpinBox(acquisitionBox);
    receiveTimeoutSpin->setRange(100, 120000);
    receiveTimeoutSpin->setValue(10000);
    receiveTimeoutSpin->setSingleStep(1000);
    auto* frameCountSpin = new QSpinBox(acquisitionBox);
    frameCountSpin->setRange(1, 200);
    frameCountSpin->setValue(10);
    auto* profileButton = new QPushButton("Profile Output", acquisitionBox);
    auto* setOutputButton = new QPushButton("Set Output", acquisitionBox);
    auto* grabButton = new QPushButton("Grab One", acquisitionBox);

    acquisitionControlsLayout->addWidget(new QLabel("Source", acquisitionControls));
    acquisitionControlsLayout->addWidget(outputSourceEdit, 4);
    acquisitionControlsLayout->addWidget(new QLabel("Receive ms", acquisitionControls));
    acquisitionControlsLayout->addWidget(receiveTimeoutSpin, 1);
    acquisitionControlsLayout->addWidget(new QLabel("Frames", acquisitionControls));
    acquisitionControlsLayout->addWidget(frameCountSpin, 1);
    acquisitionControlsLayout->addWidget(profileButton);
    acquisitionControlsLayout->addWidget(setOutputButton);
    acquisitionControlsLayout->addWidget(grabButton);

    auto* acquisitionBody = new QWidget(acquisitionBox);
    auto* acquisitionBodyLayout = new QHBoxLayout(acquisitionBody);
    acquisitionBodyLayout->setContentsMargins(0, 0, 0, 0);
    auto* acquisitionInfoEdit = new QPlainTextEdit(acquisitionBody);
    acquisitionInfoEdit->setReadOnly(true);
    acquisitionInfoEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    auto* imagePreviewLabel = new QLabel("No image", acquisitionBody);
    imagePreviewLabel->setAlignment(Qt::AlignCenter);
    imagePreviewLabel->setMinimumSize(320, 220);
    imagePreviewLabel->setStyleSheet("border: 1px solid #9a9a9a; background: #202020; color: #e0e0e0;");
    acquisitionBodyLayout->addWidget(acquisitionInfoEdit, 2);
    acquisitionBodyLayout->addWidget(imagePreviewLabel, 1);

    acquisitionLayout->addWidget(acquisitionControls);
    acquisitionLayout->addWidget(acquisitionBody);

    auto* resourceBox = new QGroupBox("Resource Read", central);
    auto* resourceLayout = new QHBoxLayout(resourceBox);
    auto* resourcePathEdit = new QLineEdit("/scan/visibleSensors/", resourceBox);
    auto* readButton = new QPushButton("Read", resourceBox);
    resourceLayout->addWidget(resourcePathEdit, 1);
    resourceLayout->addWidget(readButton);

    auto* devicesTable = new QTableWidget(0, 5, central);
    devicesTable->setHorizontalHeaderLabels({"IP", "Model", "Serial", "Port", "Local"});
    devicesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    devicesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    devicesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto* logHeader = new QWidget(central);
    auto* logHeaderLayout = new QHBoxLayout(logHeader);
    logHeaderLayout->setContentsMargins(0, 0, 0, 0);
    logHeaderLayout->addWidget(new QLabel("Log", logHeader));
    logHeaderLayout->addStretch(1);
    auto* clearLogButton = new QPushButton("Clear Log", logHeader);
    logHeaderLayout->addWidget(clearLogButton);

    auto* logEdit = new QPlainTextEdit(central);
    logEdit->setReadOnly(true);
    logEdit->setLineWrapMode(QPlainTextEdit::NoWrap);

    root->addWidget(connectionBox);
    root->addWidget(topRow);
    root->addWidget(scanSettingsBox);
    root->addWidget(acquisitionBox, 2);
    root->addWidget(resourceBox);
    root->addWidget(new QLabel("Discovered devices", central));
    root->addWidget(devicesTable, 2);
    root->addWidget(logHeader);
    root->addWidget(logEdit, 2);

    window.setCentralWidget(central);

    std::shared_ptr<gocator::GocatorSettingsManager> connectedSettings;

    std::vector<QPushButton*> actionButtons = {
        connectButton,
        disconnectButton,
        discoverButton,
        infoButton,
        profileButton,
        setOutputButton,
        grabButton,
        applyScanSettingsButton,
        readScannerButton,
        schemaScannerButton,
        readButton,
    };

    bool cursorBusy = false;
    auto setBusy = [&](bool busy) {
        for (QPushButton* button : actionButtons)
        {
            button->setEnabled(!busy);
        }

        if (busy && !cursorBusy)
        {
            QApplication::setOverrideCursor(Qt::BusyCursor);
            cursorBusy = true;
        }
        else if (!busy && cursorBusy)
        {
            QApplication::restoreOverrideCursor();
            cursorBusy = false;
        }
    };

    auto applyResult = [&](const OperationResult& result) {
        if (!result.devices.empty())
        {
            setDevices(*devicesTable, result.devices);
        }
        if (!result.selectedAddress.empty())
        {
            addressEdit->setText(toQString(result.selectedAddress));
        }
        if (result.clearSettings)
        {
            connectedSettings.reset();
        }
        if (result.takeSettings)
        {
            connectedSettings = result.settings;
        }
        if (result.connectedKnown)
        {
            const QString endpoint = QString("%1:%2").arg(addressEdit->text().trimmed()).arg(portSpin->value());
            setStatus(*statusValue, *endpointValue, *gdpValue, result.connected, endpoint);
        }
        if (result.hasScanner)
        {
            scannerInfoEdit->setPlainText(toQString(scannerInfoText(result.scanner)));
            if (outputSourceEdit->text().trimmed().isEmpty())
            {
                outputSourceEdit->setText(toQString(result.scanner.profileSourceId));
            }
        }
        if (result.hasFrame)
        {
            const std::string text = frameText(result.frame);
            acquisitionInfoEdit->setPlainText(toQString(text));
            frameValue->setText(QString("messages=%1 images=%2 profiles=%3")
                .arg(result.frame.messageCount)
                .arg(result.frame.images.size())
                .arg(result.frame.profiles.size()));

            if (!result.frame.images.empty())
            {
                const QImage preview = imagePreview(result.frame.images.front());
                if (!preview.isNull())
                {
                    imagePreviewLabel->setPixmap(QPixmap::fromImage(preview).scaled(
                        imagePreviewLabel->size(),
                        Qt::KeepAspectRatio,
                        Qt::SmoothTransformation));
                }
                else
                {
                    imagePreviewLabel->setText("Image format not previewed");
                }
            }
            else if (!result.frame.profiles.empty())
            {
                const QImage preview = profilePreview(result.frame.profiles.front(), imagePreviewLabel->size());
                imagePreviewLabel->setPixmap(QPixmap::fromImage(preview));
            }
            else
            {
                imagePreviewLabel->setText("No image/profile in frame");
            }
        }
    };

    auto runOperation = [&](const QString& title, std::function<OperationResult()> task) {
        appendLog(*logEdit, title + "...");
        setBusy(true);

        auto future = std::make_shared<std::future<OperationResult>>(
            std::async(std::launch::async, [task = std::move(task)] {
                try
                {
                    return task();
                }
                catch (const std::exception& e)
                {
                    return OperationResult{false, e.what()};
                }
                catch (...)
                {
                    return OperationResult{false, "Unknown error"};
                }
            }));

        auto* timer = new QTimer(&window);
        QObject::connect(timer, &QTimer::timeout, &window, [&, future, timer] {
            if (future->wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
            {
                return;
            }

            timer->stop();
            const OperationResult result = future->get();
            applyResult(result);
            appendLog(*logEdit, QString("%1: %2")
                .arg(result.ok ? "OK" : "ERROR", toQString(result.message)));
            setBusy(false);
            timer->deleteLater();
        });
        timer->start(50);
    };

    auto configOrLog = [&]() -> std::optional<gocator::GocatorConnectionConfig> {
        try
        {
            return configFromUi(*addressEdit, *portSpin, *timeoutSpin);
        }
        catch (const std::exception& e)
        {
            appendLog(*logEdit, QString("ERROR: %1").arg(e.what()));
            return std::nullopt;
        }
    };

    auto connectTemporarySettings = [](const gocator::GocatorConnectionConfig& config) {
        auto settings = std::make_shared<gocator::GocatorSettingsManager>(config);
        settings->connect();
        return settings;
    };

    QObject::connect(clearLogButton, &QPushButton::clicked, logEdit, &QPlainTextEdit::clear);

    QObject::connect(devicesTable, &QTableWidget::cellDoubleClicked, &window, [&](int row, int) {
        const QTableWidgetItem* addressItem = devicesTable->item(row, 0);
        const QTableWidgetItem* portItem = devicesTable->item(row, 3);
        if (addressItem != nullptr)
        {
            addressEdit->setText(addressItem->text());
        }
        if (portItem != nullptr)
        {
            portSpin->setValue(portItem->text().toInt());
        }
    });

    QObject::connect(discoverButton, &QPushButton::clicked, &window, [&] {
        const int timeoutMs = timeoutSpin->value();
        runOperation("Discover", [timeoutMs] {
            gocator::GocatorDiscoveryOptions options;
            options.timeoutMs = static_cast<std::uint64_t>(timeoutMs);

            const gocator::GocatorDiscovery discovery;
            OperationResult result;
            result.devices = discovery.discover(options);
            result.ok = !result.devices.empty();

            std::ostringstream out;
            out << "found=" << result.devices.size();
            for (const gocator::GocatorDeviceInfo& device : result.devices)
            {
                out << '\n'
                    << device.address
                    << " model=" << device.deviceModel
                    << " serial=" << device.serialNumber
                    << " port=" << device.controlPort
                    << " local=" << (device.canConnectLocally() ? "yes" : "no");

                if (result.selectedAddress.empty() && device.canConnectLocally())
                {
                    result.selectedAddress = device.address;
                }
            }

            result.message = out.str();
            return result;
        });
    });

    QObject::connect(connectButton, &QPushButton::clicked, &window, [&] {
        const auto config = configOrLog();
        if (!config)
        {
            return;
        }

        runOperation("Connect", [config = *config] {
            auto settings = std::make_shared<gocator::GocatorSettingsManager>(config);
            settings->connect();

            OperationResult result;
            result.ok = true;
            result.connectedKnown = true;
            result.connected = true;
            result.takeSettings = true;
            result.settings = settings;
            result.scanner = settings->detectPrimaryScanner();
            result.hasScanner = true;
            result.message = scannerInfoText(result.scanner);
            return result;
        });
    });

    QObject::connect(disconnectButton, &QPushButton::clicked, &window, [&] {
        auto settings = connectedSettings;
        runOperation("Disconnect", [settings] {
            if (settings)
            {
                settings->disconnect();
            }

            OperationResult result;
            result.ok = true;
            result.connectedKnown = true;
            result.connected = false;
            result.clearSettings = true;
            result.message = "disconnected";
            return result;
        });
    });

    QObject::connect(infoButton, &QPushButton::clicked, &window, [&] {
        const auto config = configOrLog();
        if (!config)
        {
            return;
        }

        auto existingSettings = connectedSettings;
        runOperation("Refresh Info", [&, config = *config, existingSettings] {
            auto settings = existingSettings ? existingSettings : connectTemporarySettings(config);
            OperationResult result;
            result.ok = true;
            result.connectedKnown = true;
            result.connected = settings->isConnected();
            result.scanner = settings->detectPrimaryScanner();
            result.hasScanner = true;
            result.message = scannerInfoText(result.scanner);
            return result;
        });
    });

    QObject::connect(applyScanSettingsButton, &QPushButton::clicked, &window, [&] {
        const auto config = configOrLog();
        if (!config)
        {
            return;
        }

        gocator::ScanTuningOptions options;
        options.profileMode.scanMode = scanModeSpin->value();
        options.profileMode.intensityEnabled = intensityCheck->isChecked();
        options.profileMode.uniformSpacingEnabled = uniformSpacingCheck->isChecked();
        options.updateExposure = exposureCheck->isChecked();
        options.exposure = exposureSpin->value();

        auto existingSettings = connectedSettings;
        runOperation("Apply Scan Settings", [&, config = *config, existingSettings, options] {
            auto settings = existingSettings ? existingSettings : connectTemporarySettings(config);
            gocator::ScannerInfo scanner = settings->detectPrimaryScanner();
            settings->stopIfRunning();
            settings->configureScanTuning(scanner, options);

            OperationResult result;
            result.ok = true;
            result.hasScanner = true;
            result.scanner = scanner;
            std::ostringstream out;
            out << "scanMode=" << options.profileMode.scanMode
                << " intensity=" << (options.profileMode.intensityEnabled ? "true" : "false")
                << " uniformSpacing=" << (options.profileMode.uniformSpacingEnabled ? "true" : "false");
            if (options.updateExposure)
            {
                out << " exposure=" << options.exposure;
            }
            result.message = out.str();
            return result;
        });
    });

    QObject::connect(readScannerButton, &QPushButton::clicked, &window, [&] {
        const auto config = configOrLog();
        if (!config)
        {
            return;
        }

        auto existingSettings = connectedSettings;
        runOperation("Read Scanner", [&, config = *config, existingSettings] {
            auto settings = existingSettings ? existingSettings : connectTemporarySettings(config);
            gocator::ScannerInfo scanner = settings->detectPrimaryScanner();

            OperationResult result;
            result.ok = true;
            result.hasScanner = true;
            result.scanner = scanner;
            result.message = settings->read(scanner.scannerPath).ToString();
            return result;
        });
    });

    QObject::connect(schemaScannerButton, &QPushButton::clicked, &window, [&] {
        const auto config = configOrLog();
        if (!config)
        {
            return;
        }

        auto existingSettings = connectedSettings;
        runOperation("Scanner Schema", [&, config = *config, existingSettings] {
            auto settings = existingSettings ? existingSettings : connectTemporarySettings(config);
            gocator::ScannerInfo scanner = settings->detectPrimaryScanner();

            OperationResult result;
            result.ok = true;
            result.hasScanner = true;
            result.scanner = scanner;
            result.message = settings->schema(scanner.scannerPath).ToString();
            return result;
        });
    });

    QObject::connect(profileButton, &QPushButton::clicked, &window, [&] {
        const auto config = configOrLog();
        if (!config)
        {
            return;
        }

        auto existingSettings = connectedSettings;
        runOperation("Profile Output", [&, config = *config, existingSettings] {
            auto settings = existingSettings ? existingSettings : connectTemporarySettings(config);
            OperationResult result;
            result.scanner = settings->prepareProfileOutput();
            result.hasScanner = true;
            result.ok = true;
            result.message = "configured\n" + scannerInfoText(result.scanner);
            return result;
        });
    });

    QObject::connect(setOutputButton, &QPushButton::clicked, &window, [&] {
        const auto config = configOrLog();
        if (!config)
        {
            return;
        }

        const std::string source = outputSourceEdit->text().trimmed().toStdString();
        auto existingSettings = connectedSettings;
        runOperation("Set Output", [&, config = *config, existingSettings, source] {
            if (source.empty())
            {
                throw std::invalid_argument("Output source is empty");
            }

            auto settings = existingSettings ? existingSettings : connectTemporarySettings(config);
            settings->stopIfRunning();
            settings->enableGocatorProtocol(true);
            settings->clearGocatorOutputs();
            settings->addOutput(source);

            OperationResult result;
            result.ok = true;
            result.message = "output=" + source;
            return result;
        });
    });

    QObject::connect(grabButton, &QPushButton::clicked, &window, [&] {
        const auto config = configOrLog();
        if (!config)
        {
            return;
        }

        const int receiveTimeoutMs = receiveTimeoutSpin->value();
        const int frameCount = frameCountSpin->value();
        runOperation("Grab One", [config = *config, receiveTimeoutMs, frameCount] {
            gocator::GocatorAcquisition acquisition(config);
            OperationResult result;
            result.frame = acquisition.grabUntilValidProfile(receiveTimeoutMs, frameCount);
            result.hasFrame = true;
            result.ok = true;
            result.message = frameText(result.frame);
            return result;
        });
    });

    QObject::connect(readButton, &QPushButton::clicked, &window, [&] {
        const auto config = configOrLog();
        if (!config)
        {
            return;
        }

        const std::string path = resourcePathEdit->text().trimmed().toStdString();
        auto existingSettings = connectedSettings;
        runOperation("Read " + resourcePathEdit->text().trimmed(), [&, config = *config, existingSettings, path] {
            auto settings = existingSettings ? existingSettings : connectTemporarySettings(config);
            OperationResult result;
            result.ok = true;
            result.message = settings->read(path).ToString();
            return result;
        });
    });

    setStatus(*statusValue, *endpointValue, *gdpValue, false, {});
    appendLog(*logEdit, "Ready");
    window.show();

    return app.exec();
}
