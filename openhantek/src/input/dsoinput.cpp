#include "dsoinput.h"
#include <QtCore>

static QString filePath = "E:\\nzm_mobile_code2\\NZMobile\\Saved\\Logs\\NZM.log";
//static QString filePath = "E:\\nzm_release2\\NZMobile\\Saved\\Logs\\NZM.log";
//static QString filePath = "D:\\Users\\hayli\\Documents\\Unreal Projects\\alsv\\Saved\\Logs\\ALSV4_0.log";
static QString KeyTemplate = "ScopeData: ";

static bool isScopeData(QString& s)
{
    int index = s.indexOf(KeyTemplate);
    if(index != -1)
    {
        s = s.trimmed();
        s = s.mid(index + KeyTemplate.size());
        return true;
    }
    return false;
}

static bool isSep(const QChar& ch)
{
    static QSet<QChar> seps = {
        ':',
        ',',
    };
    return seps.contains(ch);
}

static bool isDot(const QChar& ch)
{
    return ch == '.';
}

QVector<QString> LexicalParser(const QString& s)
{
    QVector<QString> tokens;
    int startPos = 0;
    int maxLen = s.size();
    while(startPos < maxLen)
    {
        const QChar ch = s[startPos];
        if(ch.isSpace())
        {
        }
        else if(ch.isLetter())
        {
            int scanPos = startPos + 1;
            while(scanPos < maxLen && s[scanPos].isLetterOrNumber())
                ++scanPos;
            tokens.push_back(s.mid(startPos, scanPos-startPos));
            startPos = scanPos - 1;
        }
        else if(ch.isDigit() || isDot(ch) || ch == '-')
        {
            bool hasDot = isDot(ch);
            int scanPos = startPos + 1;
            while(scanPos < maxLen)
            {
                if(isDot(s[scanPos]))
                {
                    if(hasDot)
                    {
                        break;
                    }
                    hasDot = true;
                }
                else if(!s[scanPos].isDigit())
                {
                    break;
                }
                ++scanPos;
            }
            tokens.push_back(s.mid(startPos, scanPos-startPos));
            startPos = scanPos - 1;
        }
        else if(isSep(ch))
        {
            tokens.push_back(ch);
        }
        ++startPos;
    }
    return tokens;
}

struct AnalysedChannelData
{
    float sampleTime;
    QVector<QPair<QString, float>> datas;
};

AnalysedChannelData DataParser(const QVector<QString>& tokens)
{
    AnalysedChannelData data;
    bool bOk = false;
    if (tokens.size() > 2)
    {
        data.sampleTime = tokens[2].toFloat(&bOk);
        if(!bOk)
            return data;
    }

    for(int i=4;i+2<tokens.size();i+=4)
    {
        float v = tokens[i+2].toFloat(&bOk);
        if(!bOk)
            return data;
        data.datas.push_back(qMakePair(tokens[i], v));
    }
    return data;
}

DsoInput::DsoInput(DsoSettings *settings, int verboseLevel ):controlsettings(nullptr, 4),dsoSettings(settings)
{

}

DsoInput::~DsoInput()
{

}

void DsoInput::quitSampling()
{
    bQuit = true;
    capturing = false;
}

void DsoInput::StartSample()
{
    emit start();
}

SampleData *DsoInput::GetSampleData(const QString &name)
{
    if(sampleDatas.contains(name))
        return sampleDatas[name];
    SampleData* newData = new SampleData();
    sampleDatas.insert(name, newData);
    newData->name = name;
    if(dsoSettings)
    {
        dsoSettings->scope.AvaliableChannelNames = sampleDatas.keys().toVector();
        emit newChannelData(&dsoSettings->scope);
    }
    return newData;
}

unsigned DsoInput::getRecordLength() const
{
    return 0;
}

void DsoInput::updateInterval()
{

}

void DsoInput::convertRawDataToSamples()
{

}

void DsoInput::restoreTargets()
{

}

void DsoInput::updateSamplerateLimits()
{

}

void DsoInput::controlSetSamplerate(uint8_t sampleIndex)
{

}

void DsoInput::enableSamplingUI(bool enabled)
{

}

Dso::ErrorCode DsoInput::setSamplerate(double samplerate)
{
    result.samplerate = samplerate;
    return Dso::ErrorCode::NONE;
}

Dso::ErrorCode DsoInput::setRecordTime(double duration)
{
return Dso::ErrorCode::NONE;
}

Dso::ErrorCode DsoInput::setChannelUsed(ChannelID channel, bool used)
{
return Dso::ErrorCode::NONE;
}

Dso::ErrorCode DsoInput::setChannelInverted(ChannelID channel, bool inverted)
{
return Dso::ErrorCode::NONE;
}

Dso::ErrorCode DsoInput::setGain(ChannelID channel, double gain)
{
return Dso::ErrorCode::NONE;
}

Dso::ErrorCode DsoInput::setTriggerMode(Dso::TriggerMode mode)
{
return Dso::ErrorCode::NONE;
}

Dso::ErrorCode DsoInput::setTriggerSource(int channel)
{
return Dso::ErrorCode::NONE;
}

Dso::ErrorCode DsoInput::setTriggerSmooth(int smooth)
{
return Dso::ErrorCode::NONE;
}

Dso::ErrorCode DsoInput::setTriggerLevel(ChannelID channel, double level)
{
return Dso::ErrorCode::NONE;
}

Dso::ErrorCode DsoInput::setTriggerSlope(Dso::Slope slope)
{
return Dso::ErrorCode::NONE;
}

Dso::ErrorCode DsoInput::setTriggerPosition(double position)
{
return Dso::ErrorCode::NONE;
}

Dso::ErrorCode DsoInput::setCalFreq(double calfreq)
{
return Dso::ErrorCode::NONE;
}

void DsoInput::applySettings(DsoSettingsScope *scope)
{

}

void DsoInput::restartSampling()
{
    if(!capturing)
        return;
    if(!currFile.isReadable())
    {
        currFile.setFileName(filePath);
        currFile.open(QIODevice::ReadOnly|QIODevice::Text);
        cacheFilePosition = 0;
        result.data.resize(4);
    }
    currFile.seek(cacheFilePosition);

    int maxSize = 0;
    while(!currFile.atEnd())
    {
        QByteArray Line = currFile.readLine();
        if(!Line.endsWith('\n'))
            break;
        cacheFilePosition = currFile.pos();

        QString LineStr(Line);
        if(isScopeData(LineStr))
        {
            QVector<QString> tokens = LexicalParser(LineStr);
            AnalysedChannelData data = DataParser(tokens);
            for(const auto& Pair : data.datas)
            {
                SampleData* sampleData = GetSampleData(Pair.first);
                sampleData->addData(data.sampleTime, Pair.second, 1.0f / 60.0f);
                maxSize = std::max(maxSize, (int)sampleData->data.size());
            }
        }
    }

    for(auto itr= sampleDatas.begin(); itr!=sampleDatas.end();++itr)
    {
        itr.value()->addEmptyData(maxSize);
    }

    for(unsigned channel = 0; channel < dsoSettings->scope.maxChannels; ++channel)
    {
        if(dsoSettings->scope.voltage[channel].used)
        {
            QString dataName = dsoSettings->scope.voltage[channel].selectedChannelName;
            if(!dataName.isEmpty())
                result.data[channel] = &GetSampleData(dataName)->data;
        }
    }
    emit samplesAvailable( &result );

    QTimer::singleShot(acquireInterval, this, &DsoInput::restartSampling);
}

void SampleData::addData(float time, float value, float frameRate)
{
    float interval = time - timeStamp;
    int count = qRound(interval / frameRate);
    if(count > 1)
    {
        double last = data.size() > 0 ? data[data.size()-1] : 0.0f;
//        for(int i = 0; i< count-1;++i)
//        {
//            data.push_back(last);
//        }
    }
    else if(count < 1)
    {

    }
    data.push_back(value);
    timeStamp = time;
}

void SampleData::addEmptyData(int num)
{
    float lastValue = data.empty() ? 0.0f : data[data.size() - 1];
    while(data.size() + 1 < static_cast<size_t>(num))
    {
        data.push_back(lastValue);
    }
}
