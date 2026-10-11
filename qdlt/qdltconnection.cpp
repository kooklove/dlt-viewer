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
 * \author Alexander Wenzel <alexander.aw.wenzel@bmw.de> 2011-2012
 *
 * \file qdlt.cpp
 * For further information see http://www.covesa.global/.
 * @licence end@
 */

#include <QtDebug>
#include <QRegularExpression>
#include <QDateTime>

#include "qdltconnection.h"

extern "C"
{
#include "dlt_common.h"
}

QDltConnection::QDltConnection()
{
    sendSerialHeader = false;
    syncSerialHeader = false;

    clear();
}

QDltConnection::~QDltConnection()
{

}

void QDltConnection::setSendSerialHeader(bool _sendSerialHeader)
{
    sendSerialHeader = _sendSerialHeader;
}

bool QDltConnection::getSendSerialHeader() const
{
    return sendSerialHeader;
}

void QDltConnection::setSyncSerialHeader(bool _syncSerialHeader)
{
    syncSerialHeader = _syncSerialHeader;
}

bool QDltConnection::getSyncSerialHeader() const
{
    return syncSerialHeader;
}

void QDltConnection::clear()
{
    data.clear();
    dataView.align(data);
    bytesReceived = 0;
    bytesError = 0;
    syncFound = 0;
    messageCounter = 0;
}

void QDltConnection::add(const QByteArray &bytes)
{
    bytesReceived += bytes.size();

    QByteArray dataViewArray = dataView;
    data = dataViewArray + bytes;

    dataView.align(data);
}

bool QDltConnection::parseDlt(QDltMsg &msg , bool supportDLTv2)
{
    /* if sync to serial header search for header */
    int found = 0;
    int firstPos = 0;
    int secondPos = 0;

    char lastFound = 0;

    /* Use primitive buffer for faster access */
    int cbuf_sz = dataView.size();
    const char *cbuf = dataView.constData();

    /* find marker in buffer */
    for(int num=0;num<cbuf_sz;num++) {
        if(cbuf[num] == 'D')
        {
            lastFound = 'D';
        }
        else if(lastFound == 'D' && cbuf[num] == 'L')
        {
            lastFound = 'L';
        }
        else if(lastFound == 'L' && cbuf[num] == 'S')
        {
            lastFound = 'S';
        }
        else if(lastFound == 'S' && cbuf[num] == 0x01)
        {
            /* header found */
            found++;
            if(found==1)
            {
                firstPos = num+1;
                syncFound++;
            }
            if(found==2)
            {
                secondPos = num+1;
                break;
            }
            lastFound = 0;
        }
        else
        {
            lastFound = 0;
        }
        if((!syncSerialHeader) && num==3)
            break;
    }

    if(syncSerialHeader && !found)
    {
        /* complete sync header not found */
        if(!lastFound)
        {
            /* clear buffer if even not start of sync header found */
            bytesError += dataView.size();
            dataView.clear();
        }
        return false;
    }

    //qDebug() << "found " << found << " firstPos " << firstPos << " secondPos " << secondPos;

    if(found==2)
    {
        /* two sync headers found */
        /* try to read msg */
        if(!msg.setMsg(dataView.mid(firstPos,secondPos-firstPos-4),false,supportDLTv2))
        {
            /* no valid msg found, perhaps to short */
            dataView.advance(secondPos-4);
            /* errors found */
            bytesError += secondPos-4;
            return false;
        }
        else
        {
            /* msg read successful */
            dataView.advance(secondPos-4);
            if(firstPos>4)
                /* errors found */
                bytesError += firstPos-4;
            return true;
        }

    }

    if(firstPos>4)
        /* errors found */
        bytesError += firstPos-4;

    /* try to read msg */
    if(!msg.setMsg(dataView.mid(firstPos),false,supportDLTv2))
    {
        /* no complete msg found */
        /* perhaps not completely received */
        /* check valid size */
        if(dataView.size()>DLT_MAX_MESSAGE_LEN)
        {
            /* size exceeds max DLT message size */
            /* clear buffer */
            /* errors found */
            bytesError += dataView.size();
            dataView.clear();
        }
        return false;
    }

    /* msg read successful */
    dataView.advance(firstPos+msg.getHeaderSize()+msg.getPayloadSize());
    return true;
}

bool QDltConnection::parseAscii(QDltMsg &msg)
{
    bool success = false;

    /* Use primitive buffer for faster access */
    int cbuf_sz = dataView.size();
    const char *cbuf = dataView.constData();

    /* find end of line in buffer */
    for(int num=0;num<cbuf_sz;num++) {
        if(cbuf[num] == '\r' || cbuf[num] == '\n')
        {
            // end of line found

            // check if line is empty, do not store empty lines
            if(num!=0)
            {
                // set parameters of DLT message to be generated
                msg.clear();
                msg.setEcuid("");
                msg.setApid("SER");
                msg.setCtid("ASC");
                msg.setMode(QDltMsg::DltModeVerbose);
                msg.setType(QDltMsg::DltTypeLog);
                msg.setSubtype(QDltMsg::DltLogInfo);
                msg.setMessageCounter(messageCounter++);
                msg.setNumberOfArguments(1);

                // add one argument as String
                QDltArgument arg;
                arg.setTypeInfo(QDltArgument::DltTypeInfoStrg);
                arg.setEndianness(QDlt::DltEndiannessLittleEndian);
                arg.setOffsetPayload(0);
                arg.setData(QByteArray(cbuf,num)+QByteArray("",1));
                msg.addArgument(arg);

                // generate binary payload and header of DLT message
                msg.genMsg();

                // successful found a new line to be written as DLT message
                success = true;
            }

            // remove parsed line from buffer
            if( (num < (cbuf_sz-1)) && (cbuf[num+1] == '\n' || cbuf[num+1] == '\r'))
            {
                // \n and \r found, remove two additional characters
                dataView.advance(num+2);
            }
            else
            {
                // only \n or \r found, remove only one character
                dataView.advance(num+1);
            }

            // msg read successful
            return success;
        }
    }

    // no message found
    return success;
}

bool QDltConnection::parseLogcatLine(const QString &line, QDltMsg &msg, const QString &defaultEcuId, unsigned char msgCounter)
{
    QString trimmed = line.trimmed();
    if(trimmed.isEmpty())
        return false;

    // Default parameters
    msg.clear();
    msg.setEcuid(defaultEcuId.isEmpty() ? QString("ANDR") : defaultEcuId.left(4));
    msg.setMode(QDltMsg::DltModeVerbose);
    msg.setType(QDltMsg::DltTypeLog);
    msg.setSubtype(QDltMsg::DltLogInfo);
    msg.setMessageCounter(msgCounter);
    msg.setNumberOfArguments(1);

    static const QRegularExpression reThreadTime(
        QStringLiteral(R"(^(?:(\d{4})-)?(\d{2})-(\d{2})\s+(\d{2}):(\d{2}):(\d{2})\.(\d{3,6})\s+(\d+)\s+(\d+)\s+([VDIWEFA])\s+([^:]*?)\s*:\s*(.*)$)")
    );
    static const QRegularExpression reBriefTime(
        QStringLiteral(R"(^(?:(\d{4})-)?(\d{2})-(\d{2})\s+(\d{2}):(\d{2}):(\d{2})\.(\d{3,6})\s+([VDIWEFA])/([^\(]+?)\s*\(\s*(\d+)\s*\)\s*:\s*(.*)$)")
    );

    auto mapPriority = [](QChar p) -> QDltMsg::DltLogDef {
        switch(p.toLatin1()) {
            case 'V': return QDltMsg::DltLogVerbose;
            case 'D': return QDltMsg::DltLogDebug;
            case 'I': return QDltMsg::DltLogInfo;
            case 'W': return QDltMsg::DltLogWarn;
            case 'E': return QDltMsg::DltLogError;
            case 'F':
            case 'A': return QDltMsg::DltLogFatal;
            default:  return QDltMsg::DltLogInfo;
        }
    };

    QRegularExpressionMatch match = reThreadTime.match(trimmed);
    if(match.hasMatch())
    {
        int year = match.captured(1).isEmpty() ? QDate::currentDate().year() : match.captured(1).toInt();
        int month = match.captured(2).toInt();
        int day = match.captured(3).toInt();
        int hour = match.captured(4).toInt();
        int minute = match.captured(5).toInt();
        int second = match.captured(6).toInt();
        QString msStr = match.captured(7);
        int ms = msStr.left(3).toInt();
        int usec = msStr.length() > 3 ? msStr.left(6).rightJustified(6, '0').toInt() : ms * 1000;

        QDateTime dt(QDate(year, month, day), QTime(hour, minute, second, ms), Qt::LocalTime);
        if(dt.isValid()) {
            msg.setTime(static_cast<unsigned int>(dt.toSecsSinceEpoch()));
            msg.setMicroseconds(usec);
        }

        uint pid = match.captured(8).toUInt();
        QString tidStr = match.captured(9);
        QChar priority = match.captured(10).at(0);
        QString tag = match.captured(11).trimmed();
        QString content = match.captured(12);

        msg.setSubtype(mapPriority(priority));
        msg.setSessionid(pid);

        QString cleanTag = tag;
        cleanTag.remove(QRegularExpression("[^a-zA-Z0-9_]"));
        QString apid = cleanTag.left(4).toUpper();
        if(apid.isEmpty()) apid = "LOGC";
        msg.setApid(apid);

        QString ctid = QString("%1").arg(tidStr.toUInt(), 4, 10, QChar('0')).right(4);
        msg.setCtid(ctid);

        QString displayPayload = QString("[%1] %2").arg(tag, content);
        QByteArray payloadData = displayPayload.toUtf8();
        payloadData.append('\0');

        QDltArgument arg;
        arg.setTypeInfo(QDltArgument::DltTypeInfoStrg);
        arg.setEndianness(QDlt::DltEndiannessLittleEndian);
        arg.setOffsetPayload(0);
        arg.setData(payloadData);
        msg.addArgument(arg);

        msg.genMsg();
        return true;
    }

    match = reBriefTime.match(trimmed);
    if(match.hasMatch())
    {
        int year = match.captured(1).isEmpty() ? QDate::currentDate().year() : match.captured(1).toInt();
        int month = match.captured(2).toInt();
        int day = match.captured(3).toInt();
        int hour = match.captured(4).toInt();
        int minute = match.captured(5).toInt();
        int second = match.captured(6).toInt();
        QString msStr = match.captured(7);
        int ms = msStr.left(3).toInt();
        int usec = msStr.length() > 3 ? msStr.left(6).rightJustified(6, '0').toInt() : ms * 1000;

        QDateTime dt(QDate(year, month, day), QTime(hour, minute, second, ms), Qt::LocalTime);
        if(dt.isValid()) {
            msg.setTime(static_cast<unsigned int>(dt.toSecsSinceEpoch()));
            msg.setMicroseconds(usec);
        }

        QChar priority = match.captured(8).at(0);
        QString tag = match.captured(9).trimmed();
        uint pid = match.captured(10).toUInt();
        QString content = match.captured(11);

        msg.setSubtype(mapPriority(priority));
        msg.setSessionid(pid);

        QString cleanTag = tag;
        cleanTag.remove(QRegularExpression("[^a-zA-Z0-9_]"));
        QString apid = cleanTag.left(4).toUpper();
        if(apid.isEmpty()) apid = "LOGC";
        msg.setApid(apid);
        msg.setCtid("MAIN");

        QString displayPayload = QString("[%1] %2").arg(tag, content);
        QByteArray payloadData = displayPayload.toUtf8();
        payloadData.append('\0');

        QDltArgument arg;
        arg.setTypeInfo(QDltArgument::DltTypeInfoStrg);
        arg.setEndianness(QDlt::DltEndiannessLittleEndian);
        arg.setOffsetPayload(0);
        arg.setData(payloadData);
        msg.addArgument(arg);

        msg.genMsg();
        return true;
    }

    // Fallback: general line
    msg.setApid("LOGC");
    msg.setCtid("LINE");
    msg.setTime(static_cast<unsigned int>(QDateTime::currentSecsSinceEpoch()));
    QByteArray payloadData = trimmed.toUtf8();
    payloadData.append('\0');

    QDltArgument arg;
    arg.setTypeInfo(QDltArgument::DltTypeInfoStrg);
    arg.setEndianness(QDlt::DltEndiannessLittleEndian);
    arg.setOffsetPayload(0);
    arg.setData(payloadData);
    msg.addArgument(arg);

    msg.genMsg();
    return true;
}

QString QDltConnection::formatLogcatLine(const QDltMsg &msg)
{
    QDateTime dt = QDateTime::fromSecsSinceEpoch(msg.getTime(), Qt::LocalTime);
    if(!dt.isValid()) {
        dt = QDateTime::currentDateTime();
    }
    int ms = (msg.getMicroseconds() / 1000) % 1000;
    QString timeStr = QString("%1.%2").arg(dt.toString("MM-dd HH:mm:ss")).arg(ms, 3, 10, QChar('0'));

    unsigned int pid = msg.getSessionid();
    QString tidStr = msg.getCtid().trimmed();
    if(tidStr.isEmpty()) tidStr = QString::number(pid);

    QChar priority = 'I';
    switch(msg.getSubtype()) {
        case QDltMsg::DltLogVerbose: priority = 'V'; break;
        case QDltMsg::DltLogDebug:   priority = 'D'; break;
        case QDltMsg::DltLogInfo:    priority = 'I'; break;
        case QDltMsg::DltLogWarn:    priority = 'W'; break;
        case QDltMsg::DltLogError:   priority = 'E'; break;
        case QDltMsg::DltLogFatal:   priority = 'F'; break;
        default:                     priority = 'I'; break;
    }

    QString payloadStr = const_cast<QDltMsg&>(msg).toStringPayload().trimmed();
    QString tag = msg.getApid().trimmed();
    QString content = payloadStr;

    if(payloadStr.startsWith('[')) {
        int closeIdx = payloadStr.indexOf(']');
        if(closeIdx > 1) {
            tag = payloadStr.mid(1, closeIdx - 1);
            content = payloadStr.mid(closeIdx + 1);
            if(content.startsWith(' '))
                content = content.mid(1);
        }
    }
    if(tag.isEmpty()) tag = "LOGC";

    return QString("%1 %2 %3 %4 %5: %6")
        .arg(timeStr)
        .arg(pid, 5)
        .arg(tidStr, 5)
        .arg(priority)
        .arg(tag, -8)
        .arg(content);
}

bool QDltConnection::parseLogcat(QDltMsg &msg, const QString &defaultEcuId)
{
    int cbuf_sz = dataView.size();
    const char *cbuf = dataView.constData();

    for(int num = 0; num < cbuf_sz; ++num) {
        if(cbuf[num] == '\r' || cbuf[num] == '\n') {
            bool success = false;
            QString line;
            if(num != 0) {
                line = QString::fromUtf8(cbuf, num);
                success = parseLogcatLine(line, msg, defaultEcuId, messageCounter++);
            }

            if((num < (cbuf_sz - 1)) && (cbuf[num + 1] == '\n' || cbuf[num + 1] == '\r')) {
                dataView.advance(num + 2);
            } else {
                dataView.advance(num + 1);
            }

            if(success) {
                lastLogcatLine = line.trimmed();
                return true;
            }
            // If empty line, continue scanning buffer for next line
            return parseLogcat(msg, defaultEcuId);
        }
    }

    return false;
}

