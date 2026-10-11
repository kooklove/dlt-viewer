/**
 * @licence app begin@
 * Copyright (C) 2011-2012  BMW AG
 *
 * This file is part of COVESA Project Dlt Viewer.
 *
 * Contributions are licensed to the COVESA Alliance under one or more
 * Contribution License Agreements.
 *
 * \copyright
 * This Source Code Form is subject to the terms of the
 * Mozilla Public License, v. 2.0. If a  copy of the MPL was not distributed with
 * this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * \file ecudialog.cpp
 * For further information see http://www.covesa.global/.
 * @licence end@
 */

#include "ecudialog.h"
#include "ui_ecudialog.h"

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QNetworkInterface>
#include <QFileDialog>
#include <QProcess>
#include <QMessageBox>

EcuDialog::EcuDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EcuDialog)
{
    ui->setupUi(this);

    ui->comboBoxPortSerial->setEditable(true);

#if defined(Q_OS_UNIX) || defined(qdoc)
    ui->comboBoxBaudrate->addItem(QLatin1String("50"), 50 );                //POSIX ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("75"), 75 );                //POSIX ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("134"), 134 );              //POSIX ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("150"), 150 );              //POSIX ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("200"), 200 );              //POSIX ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("1800"), 1800 );            //POSIX ONLY
#  if (defined(B230400) && defined(B4000000)) || defined(qdoc)
    ui->comboBoxBaudrate->addItem(QLatin1String("230400"), 230400 );        //POSIX ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("460800"), 460800 );        //POSIX ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("500000"), 500000 );        //POSIX ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("576000"), 576000 );       //POSIX ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("921600"), 921600 );        //POSIX ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("100000"), 1000000 );      //POSIX ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("115200"), 1152000 );      //POSIX ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("150000"), 1500000 );      //POSIX ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("200000"), 2000000 );      //POSIX ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("250000"), 2500000 );     //POSIX ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("300000"), 3000000 );      //POSIX ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("350000"), 3500000 );      //POSIX ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("400000"), 4000000 );      //POSIX ONLY
#  endif
#endif //Q_OS_UNIX

#if defined(Q_OS_WIN) || defined(qdoc)
    ui->comboBoxBaudrate->addItem(QLatin1String("14400"), 14400 );          //WINDOWS ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("56000"), 56000 );          //WINDOWS ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("28000"), 128000 );        //WINDOWS ONLY
    ui->comboBoxBaudrate->addItem(QLatin1String("256000"), 256000 );       //WINDOWS ONLY
#endif  //Q_OS_WIN
    ui->comboBoxBaudrate->addItem(QLatin1String("110"), 110 );
    ui->comboBoxBaudrate->addItem(QLatin1String("300"), 300 );
    ui->comboBoxBaudrate->addItem(QLatin1String("600"), 600 );
    ui->comboBoxBaudrate->addItem(QLatin1String("1200"), QSerialPort::Baud1200 );
    ui->comboBoxBaudrate->addItem(QLatin1String("2400"), QSerialPort::Baud2400 );
    ui->comboBoxBaudrate->addItem(QLatin1String("4800"), QSerialPort::Baud4800 );
    ui->comboBoxBaudrate->addItem(QLatin1String("9600"), QSerialPort::Baud9600 );
    ui->comboBoxBaudrate->addItem(QLatin1String("19200"), QSerialPort::Baud19200 );
    ui->comboBoxBaudrate->addItem(QLatin1String("38400"),  QSerialPort::Baud38400 );
    ui->comboBoxBaudrate->addItem(QLatin1String("57600"), QSerialPort::Baud57600 );
    ui->comboBoxBaudrate->addItem(QLatin1String("115200"), QSerialPort::Baud115200 );
    ui->comboBoxBaudrate->addItem(QLatin1String("230400"), 230400 );
    ui->comboBoxBaudrate->addItem(QLatin1String("460800"), 460800 );
    ui->comboBoxBaudrate->addItem(QLatin1String("921600"), 921600 );
    ui->comboBoxBaudrate->addItem(QLatin1String("1000000"), 1000000 );

    ui->comboBoxBaudrate->setCurrentIndex(ui->comboBoxBaudrate->count()-1);

}

EcuDialog::~EcuDialog()
{
    delete ui;
}

void EcuDialog::setData(EcuItem &item)
{
    ui->idLineEdit->setText(item.id);
    ui->descriptionLineEdit->setText(item.description);
    ui->comboBoxInterface->setCurrentIndex(item.interfacetype);
    ui->comboBoxHostname->setEditText(item.getHostname());
    ui->comboBoxPortIP->setEditText(QString("%1").arg(item.getIpport()));
    ui->comboBoxPortIP_UDP->setEditText(QString("%1").arg(item.getUdpport()));
    ui->comboBoxNetworkIF->setEditText(QString("%1").arg(item.getEthIF()));
    ui->comboBoxPortSerial->setEditText(item.getPort());

    ui->comboBoxBaudrate->setCurrentIndex(ui->comboBoxBaudrate->count()-1);
    for(int i=0; i<ui->comboBoxBaudrate->count(); i++)
    {
        if(item.getBaudrate() == (QSerialPort::BaudRate)ui->comboBoxBaudrate->itemData(i).toInt())
            ui->comboBoxBaudrate->setCurrentIndex(i);
    }

    ui->loglevelComboBox->setCurrentIndex(item.loglevel);
    ui->tracestatusComboBox->setCurrentIndex(item.tracestatus);
    ui->comboBoxVerboseMode->setCurrentIndex(item.verbosemode);
    ui->checkBoxSendSerialHeaderSerial->setCheckState(item.getSendSerialHeaderSerial()?Qt::Checked:Qt::Unchecked);
    ui->checkBoxSyncToSerialHeaderSerial->setCheckState(item.getSyncSerialHeaderSerial()?Qt::Checked:Qt::Unchecked);
    ui->checkBoxTiming->setCheckState(item.timingPackets?Qt::Checked:Qt::Unchecked);

    ui->checkBoxGetLogInfo->setCheckState(item.sendGetLogInfo?Qt::Checked:Qt::Unchecked);
    ui->checkBoxDefaultLogLevel->setCheckState(item.sendDefaultLogLevel?Qt::Checked:Qt::Unchecked);
    ui->checkBoxGetSoftwareVersion->setCheckState(item.sendGetSoftwareVersion?Qt::Checked:Qt::Unchecked);

    ui->checkBoxUpdate->setCheckState(item.updateDataIfOnline?Qt::Checked:Qt::Unchecked);

    ui->checkBoxMulticast->setCheckState(item.is_multicast?Qt::Checked:Qt::Unchecked);
    ui->checkBoxAutoReconnect->setCheckState(item.autoReconnect?Qt::Checked:Qt::Unchecked);
    ui->spinBoxAutoreconnect->setValue(item.autoReconnectTimeout);

    ui->lineEditAdbPath->setText(item.getAdbPath().isEmpty() ? "adb" : item.getAdbPath());
    ui->lineEditAdbDeviceSerial->setText(item.getAdbDeviceSerial());
    ui->lineEditAdbPreCommand->setText(item.getAdbPreCommand());
    ui->lineEditAdbLogcatArgs->setText(item.getAdbLogcatArgs().isEmpty() ? "-v threadtime" : item.getAdbLogcatArgs());
    ui->comboBoxAdbSaveFormat->setCurrentIndex(item.getAdbSaveFormat());

    on_comboBoxInterface_currentIndexChanged(ui->comboBoxInterface->currentIndex());

    ui->checkBoxWriteDLTv2StorageHeader->setCheckState((item.getWriteDLTv2StorageHeader())?Qt::Checked:Qt::Unchecked);
}

void EcuDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

QString EcuDialog::id()
{
   return  ui->idLineEdit->text();
}

QString EcuDialog::description()
{
    return  ui->descriptionLineEdit->text();
}

int EcuDialog::interfacetype()
{
    return  ui->comboBoxInterface->currentIndex();
}

QString EcuDialog::hostname()
{
    return  ui->comboBoxHostname->currentText();
}

QString EcuDialog::mcastaddress()
{
    return  ui->comboBoxUDP_mcastaddress->currentText();
}

QString EcuDialog::serialPort()
{
    return  ui->comboBoxPortSerial->currentText();
}

unsigned int EcuDialog::port()
{
    switch (ui->comboBoxInterface->currentIndex())
    {
      case EcuItem::INTERFACETYPE_TCP:
           return  ui->comboBoxPortIP->currentText().toUInt();
           break;
      case EcuItem::INTERFACETYPE_UDP:
           return  ui->comboBoxPortIP_UDP->currentText().toUInt();
           break;
     default:
        break;
    }
   return -1;
}

unsigned int EcuDialog::tcpport()
{
    return  ui->comboBoxPortIP->currentText().toUInt();
}

unsigned int EcuDialog::udpport()
{
    return  ui->comboBoxPortIP_UDP->currentText().toUInt();
}


QString EcuDialog::EthInterface()
{
    return  ui->comboBoxNetworkIF->currentText();
}

QSerialPort::BaudRate EcuDialog::baudrate()
{
    return (QSerialPort::BaudRate)ui->comboBoxBaudrate->itemData(ui->comboBoxBaudrate->currentIndex()).toInt();
}

int EcuDialog::loglevel()
{
    return  ui->loglevelComboBox->currentIndex();
}

int EcuDialog::tracestatus()
{
    return  ui->tracestatusComboBox->currentIndex();
}

int EcuDialog::verbosemode()
{
    return  ui->comboBoxVerboseMode->currentIndex();
}

int EcuDialog::timingPackets()
{
    return (ui->checkBoxTiming->checkState() == Qt::Checked);
}

int EcuDialog::sendGetLogInfo()
{
    return (ui->checkBoxGetLogInfo->checkState() == Qt::Checked);
}

int EcuDialog::sendDefaultLogLevel()
{
    return (ui->checkBoxDefaultLogLevel->checkState() == Qt::Checked);
}

int EcuDialog::sendGetSoftwareVersion()
{
    return (ui->checkBoxGetSoftwareVersion->checkState() == Qt::Checked);
}

int EcuDialog::update()
{
    return  ui->checkBoxUpdate->isChecked();
}

QString EcuDialog::adbPath()
{
    return ui->lineEditAdbPath->text().trimmed();
}

QString EcuDialog::adbDeviceSerial()
{
    return ui->lineEditAdbDeviceSerial->text().trimmed();
}

QString EcuDialog::adbPreCommand()
{
    return ui->lineEditAdbPreCommand->text().trimmed();
}

QString EcuDialog::adbLogcatArgs()
{
    return ui->lineEditAdbLogcatArgs->text().trimmed();
}

int EcuDialog::adbSaveFormat()
{
    return ui->comboBoxAdbSaveFormat->currentIndex();
}

void EcuDialog::on_checkBoxMulticast_toggled(bool checked)
{
 ui->comboBoxUDP_mcastaddress->setEnabled(checked);
}

bool EcuDialog::getMulticast()
{
    return  ui->checkBoxMulticast->isChecked();
}

void EcuDialog::setMulticast(bool mcast)
{
    ui->checkBoxMulticast->setChecked(mcast);
    ui->comboBoxUDP_mcastaddress->setEnabled(mcast);
}

int EcuDialog::autoReconnect()
{
    return  ui->checkBoxAutoReconnect->isChecked();
}

int EcuDialog::autoReconnectTimeout()
{
    return  ui->spinBoxAutoreconnect->value();
}

void EcuDialog::setHostnameList(QStringList hostnames)
{
    ui->comboBoxHostname->clear();
    ui->comboBoxHostname->addItems(hostnames);
}

void EcuDialog::setSerialPortList()
{
    ui->comboBoxPortSerial->clear();

    QList<QSerialPortInfo> 	availablePorts  = QSerialPortInfo::availablePorts();
    qDebug() << "portName" << "description" << "manufacturer" << "serialNumber" << "productIdentifier" << "vendorIdentifier" << "systemLocation";
    for(int num = 0; num<availablePorts.length();num++)
    {
        qDebug() << availablePorts[num].portName() << availablePorts[num].description() << availablePorts[num].manufacturer() << availablePorts[num].serialNumber() << availablePorts[num].productIdentifier() << availablePorts[num].vendorIdentifier() << availablePorts[num].systemLocation();
        ui->comboBoxPortSerial->addItem(availablePorts[num].portName());
    }

}

void EcuDialog::setIPPortList(QStringList ports)
{
    ui->comboBoxPortIP->clear();
    ui->comboBoxPortIP->addItems(ports);
}

void EcuDialog::setUDPPortList(QStringList ports)
{
    ui->comboBoxPortIP_UDP->clear();
    ui->comboBoxPortIP_UDP->addItems(ports);
}

void EcuDialog::setNetworkIFList(QString lastsetting)
{
    ui->comboBoxNetworkIF->clear();
    QList<QNetworkInterface> 	interfaces  = QNetworkInterface::allInterfaces();;
    for(int num = 0; num<interfaces.length();num++)
    {
        if(interfaces[num].type()!=1)
           ui->comboBoxNetworkIF->addItem(interfaces[num].humanReadableName());
    }
    ui->comboBoxNetworkIF->setCurrentText(lastsetting);
}

void EcuDialog::setMulticastAddresses(QStringList mcaddresses)
{
    ui->comboBoxUDP_mcastaddress->clear();
    ui->comboBoxUDP_mcastaddress->addItems(mcaddresses);
}


void EcuDialog::setDialogToEcuItem(EcuItem *item)
{
    item->id = this->id();
    item->description = this->description();
    item->interfacetype = this->interfacetype();
    item->setHostname(this->hostname());
    item->setIpport(this->tcpport());
    item->setUdpport(this->udpport());
    item->setEthIF(this->EthInterface());
    item->setmcastIP(this->mcastaddress());
    item->setPort(ui->comboBoxPortSerial->currentText());
    item->setBaudrate(this->baudrate());
    item->loglevel = this->loglevel();
    item->tracestatus = this->tracestatus();
    item->verbosemode = this->verbosemode();
    item->timingPackets = this->timingPackets();
    item->sendGetLogInfo = this->sendGetLogInfo();
    item->sendDefaultLogLevel = this->sendDefaultLogLevel();
    item->sendGetSoftwareVersion = this->sendGetSoftwareVersion();
    item->updateDataIfOnline = this->update();
    item->is_multicast = this->getMulticast();
    item->autoReconnect = this->autoReconnect();
    item->autoReconnectTimeout = this->autoReconnectTimeout();
    item->updateAutoReconnectTimestamp();

    /* new qdlt library */
    item->ipcon.setPort(this->port());
    item->ipcon.setHostname(this->hostname());
    item->serialcon.setBaudrate(this->baudrate());
    item->serialcon.setPort(ui->comboBoxPortSerial->currentText());
    item->setSendSerialHeaderSerial(this->ui->checkBoxSendSerialHeaderSerial->checkState()==Qt::Checked);
    item->setSyncSerialHeaderSerial(this->ui->checkBoxSyncToSerialHeaderSerial->checkState()==Qt::Checked);

    item->setAdbPath(this->adbPath());
    item->setAdbDeviceSerial(this->adbDeviceSerial());
    item->setAdbPreCommand(this->adbPreCommand());
    item->setAdbLogcatArgs(this->adbLogcatArgs());
    item->setAdbSaveFormat(this->adbSaveFormat());

    item->setWriteDLTv2StorageHeader(ui->checkBoxWriteDLTv2StorageHeader->checkState()==Qt::Checked);
}

void EcuDialog::on_checkBoxAutoReconnect_toggled(bool checked)
{
    ui->spinBoxAutoreconnect->setEnabled(checked);
}

void EcuDialog::setIFpresetindex(int preset)
{
  ui->comboBoxInterface->setCurrentIndex(preset);
  on_comboBoxInterface_currentIndexChanged(preset);
}

int EcuDialog::interfacetypecurrentindex()
{
  return( ui->comboBoxInterface->currentIndex());
}


void EcuDialog::on_comboBoxInterface_currentIndexChanged(int index)
{
    switch(index)
    {
        case EcuItem::INTERFACETYPE_TCP:
            //we have TCP set -> disable others, enable TCP
            ui->tabWidget->setTabEnabled(1,true);
            ui->tabWidget->setTabEnabled(2,false);
            ui->tabWidget->setTabEnabled(3,false);
            ui->tabWidget->setTabEnabled(4,false);
            ui->tabWidget->setCurrentIndex(1);
            ui->comboBoxNetworkIF->setVisible(false);
            ui->checkBoxMulticast->setVisible(false);
            ui->label_3->setText(QString("Hostname:"));
            ui->label_3->setToolTip(QString("Name or IP address of server to connect to."));
        break;

        case EcuItem::INTERFACETYPE_UDP:
            //we have UDP set -> disable others, enable UDP
            ui->tabWidget->setTabEnabled(1,false);
            ui->tabWidget->setTabEnabled(2,true);
            ui->tabWidget->setTabEnabled(3,false);
            ui->tabWidget->setTabEnabled(4,false);
            ui->tabWidget->setCurrentIndex(2);
            ui->comboBoxNetworkIF->setVisible(true);
            ui->checkBoxMulticast->setVisible(true);
            ui->label_UDP_mcastadress->setToolTip(QString("Multicast IP address to subscribe to. \nSelect <none> for unicast."));
            ui->label_UDP_receiver->setToolTip(QString("Local adapter to receive the UDP messages.\nSelect <AnyIP> / 0.0.0.0 in case of any."));
            ui->comboBoxUDP_mcastaddress->setEnabled(ui->checkBoxMulticast->isChecked());
            break;

        case EcuItem::INTERFACETYPE_SERIAL_DLT:
            ui->tabWidget->setTabEnabled(1,false);
            ui->tabWidget->setTabEnabled(2,false);
            ui->tabWidget->setTabEnabled(3,true);
            ui->tabWidget->setTabEnabled(4,false);
            ui->tabWidget->setCurrentIndex(3);
            ui->checkBoxSendSerialHeaderSerial->setVisible(true);
            ui->checkBoxSyncToSerialHeaderSerial->setVisible(true);
            break;

        case EcuItem::INTERFACETYPE_SERIAL_ASCII:
            ui->tabWidget->setTabEnabled(1,false);
            ui->tabWidget->setTabEnabled(2,false);
            ui->tabWidget->setTabEnabled(3,true);
            ui->tabWidget->setTabEnabled(4,false);
            ui->tabWidget->setCurrentIndex(3);
            ui->checkBoxSendSerialHeaderSerial->setVisible(false);
            ui->checkBoxSyncToSerialHeaderSerial->setVisible(false);
            break;

        case EcuItem::INTERFACETYPE_ANDROID_LOGCAT:
            ui->tabWidget->setTabEnabled(1,false);
            ui->tabWidget->setTabEnabled(2,false);
            ui->tabWidget->setTabEnabled(3,false);
            ui->tabWidget->setTabEnabled(4,true);
            ui->tabWidget->setCurrentIndex(4);
            break;
    }
}

void EcuDialog::on_pushButtonBrowseAdb_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Select ADB Executable"),
        ui->lineEditAdbPath->text().isEmpty() ? QString() : ui->lineEditAdbPath->text(),
#if defined(Q_OS_WIN)
        tr("Executables (*.exe);;All Files (*)")
#else
        tr("All Files (*)")
#endif
    );
    if (!fileName.isEmpty()) {
        ui->lineEditAdbPath->setText(fileName);
    }
}

void EcuDialog::on_pushButtonRefreshAdb_clicked()
{
    QString adbExe = ui->lineEditAdbPath->text().trimmed();
    if (adbExe.isEmpty()) adbExe = "adb";

    QProcess process;
    process.start(adbExe, QStringList() << "devices");
    if (!process.waitForStarted(3000)) {
        QMessageBox::warning(this, tr("ADB Detect"), tr("Failed to start ADB: %1").arg(adbExe));
        return;
    }
    if (!process.waitForFinished(5000)) {
        process.kill();
        QMessageBox::warning(this, tr("ADB Detect"), tr("ADB timed out detecting devices."));
        return;
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput());
    QStringList lines = output.split('\n',
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        Qt::SkipEmptyParts
#else
        QString::SkipEmptyParts
#endif
    );

    QStringList devices;
    for (int i = 1; i < lines.size(); ++i) { // Skip "List of devices attached"
        QString line = lines[i].trimmed();
        if (line.isEmpty()) continue;
        QStringList parts = line.split(QRegularExpression("\\s+"));
        if (parts.size() >= 2 && parts[1] == "device") {
            devices.append(parts[0]);
        }
    }

    if (devices.isEmpty()) {
        QMessageBox::information(this, tr("ADB Detect"), tr("No connected Android devices found."));
    } else {
        ui->lineEditAdbDeviceSerial->setText(devices.first());
        QMessageBox::information(this, tr("ADB Detect"), tr("Found devices: %1\nSelected: %2").arg(devices.join(", "), devices.first()));
    }
}

void EcuDialog::on_pushButtonConnectAdbTcp_clicked()
{
    QString ip = ui->lineEditAdbIp->text().trimmed();
    if (ip.isEmpty()) {
        QMessageBox::warning(this, tr("ADB Ethernet"), tr("Please enter an IP address."));
        return;
    }

    QString port = ui->lineEditAdbPort->text().trimmed();
    if (port.isEmpty()) port = "5555";

    QString target = QString("%1:%2").arg(ip, port);
    QString adbExe = ui->lineEditAdbPath->text().trimmed();
    if (adbExe.isEmpty()) adbExe = "adb";

    QProcess process;
    process.start(adbExe, QStringList() << "connect" << target);
    if (!process.waitForStarted(3000)) {
        QMessageBox::warning(this, tr("ADB Ethernet"), tr("Failed to start ADB: %1").arg(adbExe));
        return;
    }
    if (!process.waitForFinished(5000)) {
        process.kill();
        QMessageBox::warning(this, tr("ADB Ethernet"), tr("ADB connect timed out."));
        return;
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    if (output.contains("connected to", Qt::CaseInsensitive)) {
        ui->lineEditAdbDeviceSerial->setText(target);
        QMessageBox::information(this, tr("ADB Ethernet"), tr("Successfully connected to %1\n%2").arg(target, output));
    } else {
        QMessageBox::warning(this, tr("ADB Ethernet"), tr("ADB response:\n%1").arg(output));
    }
}

