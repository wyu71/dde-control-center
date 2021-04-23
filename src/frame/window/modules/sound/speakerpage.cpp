/*
 * Copyright (C) 2011 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     lq <longqi_cm@deepin.com>
 *
 * Maintainer: lq <longqi_cm@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "speakerpage.h"
#include "modules/sound/soundmodel.h"
#include "widgets/switchwidget.h"
#include "window/utils.h"
#include "window/gsettingwatcher.h"
#include "widgets/titlelabel.h"
#include "widgets/settingsgroup.h"

#include <DStyle>
#include <DTipLabel>
#include <DFontSizeManager>
#include <DStandardItem>
#include <DGuiApplicationHelper>
#include <DApplication>

#include <QAction>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QDebug>
#include <QSvgRenderer>

using namespace dcc::sound;
using namespace dcc::widgets;
using namespace DCC_NAMESPACE::sound;

Q_DECLARE_METATYPE(const dcc::sound::Port *)

DWIDGET_USE_NAMESPACE

SpeakerPage::SpeakerPage(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout)
    , m_outputSlider(nullptr)
    , m_speakSlider(nullptr)
    , m_vbWidget(nullptr)
    , m_balanceSlider(nullptr)
    , m_lastsetvalue(0)
    , m_balance(true)
    , m_volumeBtn(nullptr)
    , m_mute(false)
    , m_enablePort(false)
{
    const int titleLeftMargin = 8;
    TitleLabel *labelOutput = new TitleLabel(tr("Output"));
    DFontSizeManager::instance()->bind(labelOutput, DFontSizeManager::T5, QFont::DemiBold);
    labelOutput->setContentsMargins(titleLeftMargin, 0, 0, 0);
    labelOutput->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_outputSoundCbx = new ComboxWidget(tr("Output Device"));
    m_outputModel  = new QStandardItemModel(m_outputSoundCbx->comboBox());
    m_outputSoundCbx->comboBox()->setModel(m_outputModel);

    m_outputSoundsGrp = new SettingsGroup(nullptr, SettingsGroup::GroupBackground);
    m_blueSoundCbx = new ComboxWidget(tr("Mode"));

    m_layout->addWidget(labelOutput);
    m_layout->setContentsMargins(ThirdPageContentsMargins);

    setLayout(m_layout);
}

SpeakerPage::~SpeakerPage()
{
    GSettingWatcher::instance()->erase("soundOutputSlider");
    GSettingWatcher::instance()->erase("soundVolumeBoost");
    GSettingWatcher::instance()->erase("soundBalanceSlider");
}

void SpeakerPage::setModel(dcc::sound::SoundModel *model)
{
    m_model = model;

    //当扬声器状态发生变化，更新设备信息显示状态
    connect(m_model, &SoundModel::isPortEnableChanged, this, [ = ](bool visible) {
        Q_UNUSED(visible);
        //启用端口后需要再判断是否启用成功后，再设置为默认端口，但因为设置端口后会有端口是否启用的状态判断，
        //导致进入死循环，所以添加判断值，判断是否是启用或禁用端口类型的操作，若是，则设置默认端口
        if (m_enablePort) {
            QModelIndex index = m_outputSoundCbx->comboBox()->view()->currentIndex();
            if (index.isValid()) {
                Q_EMIT requestSetPort(m_outputModel->data(index, Qt::WhatsThisPropertyRole).value<const dcc::sound::Port *>());
            }
        }
        showDevice();
    });

    connect(m_model, &SoundModel::setPortChanged, this, [ = ](const dcc::sound::Port  * port) {
        m_currentPort = port;
        if (!m_currentPort)
            return;
        m_enablePort = false;

        setBlueModeVisible(port->isBluetoothPort());
        Q_EMIT m_model->requestSwitchEnable(port->cardId(), port->id());//设置端口后，发送信号，判断该端口是否需要禁用
    });

    auto ports = m_model->ports();
    for (auto port : ports) {
        addPort(port);
    }

    connect(m_model, &SoundModel::bluetoothModeOptsChanged, this, [ = ](const QStringList bluetoothModeOpts){
        if (m_bluetoothModeOpts != bluetoothModeOpts) {
            m_bluetoothModeOpts = bluetoothModeOpts;
            m_blueSoundCbx->comboBox()->clear(); // 先清除避免重复
            m_blueSoundCbx->comboBox()->addItems(bluetoothModeOpts);
        }
    });
    connect(m_model, &SoundModel::portAdded, this, &SpeakerPage::addPort);
    connect(m_model, &SoundModel::portRemoved, this, &SpeakerPage::removePort);
    connect(m_model, &SoundModel::bluetoothModeChanged, m_blueSoundCbx, &ComboxWidget::setCurrentText);
    connect(m_outputSoundCbx->comboBox(), static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &SpeakerPage::changeComboxIndex);
    connect(m_model, &SoundModel::setBlanceVisible, this, [ = ](bool flag) {
            m_balance = flag;
            showDevice();
    });
    connect(m_model, &SoundModel::speakerOnChanged, this, [ = ](bool flag) {
            m_mute = flag;
            refreshIcon();
    });

    initSlider();
    initCombox();
    requestBalanceVisible();
}

void SpeakerPage::removePort(const QString &portId, const uint &cardId)
{
    auto rmFunc = [ = ](QStandardItemModel * model) {
        for (int i = 0; i < model->rowCount();) {
            auto item = model->item(i);
            auto port = item->data(Qt::WhatsThisPropertyRole).value<const dcc::sound::Port *>();
            if (port->id() == portId && cardId == port->cardId()) {
                disconnect(m_outputSoundCbx->comboBox(), static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &SpeakerPage::changeComboxIndex);
                m_outputSoundCbx->comboBox()->hidePopup();
                model->removeRow(i);
                connect(m_outputSoundCbx->comboBox(), static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &SpeakerPage::changeComboxIndex);
            } else {
                ++i;
            }
        }
    };

    rmFunc(m_outputModel);
    showDevice();
}

void SpeakerPage::changeComboxIndex(const int idx)
{
    showDevice();
    if (idx < 0)
        return;
    auto temp = m_outputModel->index(idx, 0);
    this->requestSetPort(m_outputModel->data(temp, Qt::WhatsThisPropertyRole).value<const dcc::sound::Port *>());
    qDebug() << "default sink index change, currentTerxt:" << m_outputSoundCbx->comboBox()->itemText(idx);
}

void SpeakerPage::clickLeftButton()
{
    Q_EMIT requestMute();
}

void SpeakerPage::changeBluetoothMode(const int idx)
{
    this->requstBluetoothMode(m_blueSoundCbx->comboBox()->itemText(idx));
}

void SpeakerPage::addPort(const dcc::sound::Port *port)
{
    // 若端口不可见不填加 port信息
    if (!port->isEnabled())
        return;

    if (dcc::sound::Port::Out == port->direction()) {
        qDebug() << "SpeakerPage::addPort" << port->name();
        DStandardItem *pi = new DStandardItem;
        pi->setText(port->name() + "(" + port->cardName() + ")");
        pi->setData(QVariant::fromValue<const dcc::sound::Port *>(port), Qt::WhatsThisPropertyRole);

        connect(port, &dcc::sound::Port::nameChanged, this, [ = ](const QString str) {
            pi->setText(str);
        });

        connect(port, &dcc::sound::Port::currentBluetoothPortChanged, this, [ = ](const bool isBluetooth) {
            setBlueModeVisible(isBluetooth);
        });
        connect(port, &dcc::sound::Port::isActiveChanged, this, [ = ](bool isActive) {
            pi->setCheckState(isActive ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
            if (isActive) {
                disconnect(m_outputSoundCbx->comboBox(), static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &SpeakerPage::changeComboxIndex);
                m_outputSoundCbx->comboBox()->setCurrentText(port->name() + "(" + port->cardName() + ")");
                setBlueModeVisible(port->isBluetoothPort());
                connect(m_outputSoundCbx->comboBox(), static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &SpeakerPage::changeComboxIndex);
            }
        });
        disconnect(m_outputSoundCbx->comboBox(), static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &SpeakerPage::changeComboxIndex);
        m_outputSoundCbx->comboBox()->hidePopup();
        m_outputModel->appendRow(pi);
        connect(m_outputSoundCbx->comboBox(), static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &SpeakerPage::changeComboxIndex);
        if (port->isActive()) {
            disconnect(m_outputSoundCbx->comboBox(), static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &SpeakerPage::changeComboxIndex);
            m_outputSoundCbx->comboBox()->setCurrentText(port->name() + "(" + port->cardName() + ")");
            setBlueModeVisible(port->isBluetoothPort());
            connect(m_outputSoundCbx->comboBox(), static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &SpeakerPage::changeComboxIndex);
            m_currentPort = port;
            Q_EMIT m_model->requestSwitchEnable(port->cardId(), port->id());
        }

        showDevice();
    }
}

void SpeakerPage::initSlider()
{
    //~ contents_path /sound/Speaker
    m_outputSlider = new TitledSliderItem(tr("Output Volume"), this);
    m_outputSlider->addBackground();
    m_speakSlider = m_outputSlider->slider();

    if (m_model->MaxUIVolume() > 1.0) {
        m_speakSlider->setSeparateValue(100);
    } else {
        m_speakSlider->setSeparateValue(0);
    }

    //初始化音量设置滚动条
    qDebug() << "max volume:" << m_model->MaxUIVolume();
    int maxRange = static_cast<int>(m_model->MaxUIVolume() * 100.0f + 0.000001);
    m_speakSlider->setRange(0, maxRange);
    m_speakSlider->setType(DCCSlider::Vernier);
    m_speakSlider->setTickPosition(QSlider::NoTicks);

    m_volumeBtn = new SoundLabel(this);
    QGridLayout *gridLayout = dynamic_cast<QGridLayout *>(m_outputSlider->slider()->layout());
    if (gridLayout) {
        gridLayout->addWidget(m_volumeBtn, 1, 0, Qt::AlignVCenter);
    }
    m_volumeBtn->setAccessibleName("volume-button");
    m_volumeBtn->setFixedSize(ICON_SIZE, ICON_SIZE);
    m_volumeBtn->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
    //从DStyle 中获取标准图标
    auto icon_high = qobject_cast<DStyle *>(style())->standardIcon(DStyle::SP_MediaVolumeHighElement);
    m_outputSlider->setRightIcon(icon_high);
    m_outputSlider->setIconSize(QSize(24, 24));

    m_speakSlider->setTickInterval(1);
    qDebug() << "speaker volume:" << m_model->speakerVolume();
    int val = static_cast<int>(m_model->speakerVolume() * 100.0f + 0.000001);
    if (val > maxRange) {
        val = maxRange;
    }
    m_speakSlider->setValue(val);
    m_outputSlider->setValueLiteral(QString::number(int(val)) + "%");
    m_speakSlider->setPageStep(1);

    //处理滑块位置变化的槽
    auto slotfunc1 = [ = ](int pos) {
        //当前设置音量大于上一次设置的音量时，说明音量是在增加
        if (pos > m_lastsetvalue) {
            //因为增加5%幅度的区间是110～120,所以要包含110,不包含120,从120开始就是1%的幅度增加
            if (pos >= 110 && pos < 120) {
                m_outputSlider->slider()->qtSlider()->setSingleStep(5);
            } else {
                m_outputSlider->slider()->qtSlider()->setSingleStep(1);
            }
            m_lastsetvalue = pos;
            //当前设置音量小于上一次设置的音量时，说明音量是在降低
        } else if (pos < m_lastsetvalue) {
            //因为增加5%幅度的区间是110～120,所以要不包含121和110,从120～110开始就是5%的幅度降低
            if (pos > 110 && pos < 121) {
                m_outputSlider->slider()->qtSlider()->setSingleStep(5);
            } else {
                m_outputSlider->slider()->qtSlider()->setSingleStep(1);
            }
            m_lastsetvalue = pos;
        }
        double vals = pos / 100.0;
        //滑块位置改变时，发送设置音量的信号
        Q_EMIT requestSetSpeakerVolume(vals);
        Q_EMIT requestMute(false);
    };
    //当点击滑槽时不会有，sliderMoved消息，用这个补
    connect(m_speakSlider, &DCCSlider::valueChanged, slotfunc1);
    //滑块移动消息处理
    connect(m_speakSlider, &DCCSlider::sliderMoved, slotfunc1);
    //当扬声器开/关时，显示/隐藏控件
    //当底层数据改变后，更新滑动条显示的数据
    connect(m_model, &SoundModel::speakerVolumeChanged, this, [ = ](double v) {
        m_speakSlider->blockSignals(true);
        m_speakSlider->setValue(v * 100 + 0.000001);
        m_speakSlider->blockSignals(false);
        m_outputSlider->setValueLiteral(QString::number((int)(v * 100 + 0.000001)) + "%");
    });

    connect(m_model, &SoundModel::maxUIVolumeChanged, this, [ = ](double maxvalue) {
        m_speakSlider->setRange(0, static_cast<int>(maxvalue * 100 + 0.000001));
        if (maxvalue > 1.0) {
            qDebug() << m_outputSlider << maxvalue;
            m_speakSlider->setSeparateValue(100);
        } else {
            m_speakSlider->setSeparateValue(0);
        }
        m_outputSlider->update();

        m_speakSlider->blockSignals(true);
        m_speakSlider->setValue(static_cast<int>(m_model->speakerVolume() * 100 + 0.000001));
        m_speakSlider->blockSignals(false);
        m_outputSlider->setValueLiteral(QString::number((int)(m_model->speakerVolume() * 100 + 0.000001))+ "%");
    });
    connect(m_volumeBtn, &SoundLabel::clicked, this, &SpeakerPage::clickLeftButton);
    m_layout->insertWidget(2, m_outputSlider);

    //音量增强
    auto hlayout = new QVBoxLayout();
    auto volumeBoost = new SwitchWidget(this);
    volumeBoost->addBackground();
    volumeBoost->setChecked(m_model->isIncreaseVolume());
    volumeBoost->setTitle(tr("Volume Boost"));
    connect(m_model, &SoundModel::increaseVolumeChanged, volumeBoost, &SwitchWidget::setChecked);
    connect(volumeBoost, &SwitchWidget::checkedChanged, this, &SpeakerPage::requestIncreaseVolume);
    hlayout->addWidget(volumeBoost);
    hlayout->setContentsMargins(0, 0, 0, 0);

    //下方提示
    auto volumeBoostTip = new DTipLabel(tr("If the volume is louder than 100%, it may distort audio and be harmful to your speaker"), this);
    volumeBoostTip->setWordWrap(true);
    volumeBoostTip->setAlignment(Qt::AlignLeft);
    volumeBoostTip->setContentsMargins(10, 0, 0, 0);
    hlayout->addWidget(volumeBoostTip);
    m_vbWidget = new QWidget(this);
    m_vbWidget->setLayout(hlayout);
    m_vbWidget->setVisible(m_model->isPortEnable());
    m_layout->insertWidget(3, m_vbWidget);
    m_layout->addWidget(m_vbWidget);    //~ contents_path /sound/Speaker
    m_balanceSlider = new TitledSliderItem(tr("Left/Right Balance"), this);
    m_balanceSlider->addBackground();

    //信号处理与上面一致
    QStringList balanceList;
    balanceList << tr("Left") << " ";
    balanceList << tr("Right");
    DCCSlider *slider2 = m_balanceSlider->slider();
    slider2->setRange(-100, 100);
    slider2->setType(DCCSlider::Vernier);
    slider2->setTickPosition(QSlider::TicksBelow);
    slider2->setTickInterval(1);
    slider2->setSliderPosition(static_cast<int>(m_model->speakerBalance() * 100 + 0.000001));
    slider2->setPageStep(1);
    m_balanceSlider->setAnnotations(balanceList);

    auto slotfunc2 = [ = ](int pos) {
        double value = pos / 100.0;
        Q_EMIT requestSetSpeakerBalance(value);
    };
    connect(slider2, &DCCSlider::valueChanged, slotfunc2);
    connect(slider2, &DCCSlider::sliderMoved, slotfunc2);
    connect(m_model, &SoundModel::speakerBalanceChanged, this, [ = ](double v) {
        slider2->blockSignals(true);
        slider2->setSliderPosition(static_cast<int>(v * 100 + 0.000001));
        slider2->blockSignals(false);
    });
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, &SpeakerPage::refreshIcon);
    connect(qApp, &DApplication::iconThemeChanged, this, &SpeakerPage::refreshIcon);

    m_layout->insertWidget(4, m_balanceSlider);
    refreshIcon();
    showDevice();

    // 使用GSettings来控制显示状态
    GSettingWatcher::instance()->bind("soundOutputSlider", m_outputSlider);
    GSettingWatcher::instance()->bind("soundVolumeBoost", volumeBoost);
    GSettingWatcher::instance()->bind("soundVolumeBoost", volumeBoostTip);
    GSettingWatcher::instance()->bind("soundBalanceSlider", m_balanceSlider);
}

void SpeakerPage::initCombox()
{
    m_blueSoundCbx->comboBox()->addItems(m_model->bluetoothAudioModeOpts());
    m_blueSoundCbx->comboBox()->setCurrentText(m_model->currentBluetoothAudioMode());
    connect(m_blueSoundCbx->comboBox(), static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &SpeakerPage::changeBluetoothMode);

    m_outputSoundsGrp->getLayout()->setContentsMargins(ThirdPageCmbMargins);
    m_outputSoundsGrp->appendItem(m_outputSoundCbx);
    m_outputSoundsGrp->appendItem(m_blueSoundCbx);

    QModelIndex index = m_outputModel->index(m_outputSoundCbx->comboBox()->currentIndex(), 0);
    const dcc::sound::Port * cunPort =  m_outputModel->data(index, Qt::WhatsThisPropertyRole).value<const dcc::sound::Port *>();
    if (index.isValid()) {
        setBlueModeVisible(cunPort->isBluetoothPort());
        m_blueSoundCbx->comboBox()->setCurrentText(m_model->currentBluetoothAudioMode());
    }

    if (m_outputSoundsGrp->layout())
        m_outputSoundsGrp->layout()->setContentsMargins(ThirdPageCmbMargins);

    m_layout->addWidget(m_outputSoundsGrp);
    m_layout->setSpacing(10);
    m_layout->addStretch(10);
}

void SpeakerPage::refreshIcon()
{
    if (m_mute) {
        m_volumeBtn->setIcon(qobject_cast<DStyle *>(style())->standardIcon(DStyle::SP_MediaVolumeMutedElement));
    } else {
        m_volumeBtn->setIcon(qobject_cast<DStyle *>(style())->standardIcon(DStyle::SP_MediaVolumeLowElement));
    }
}

/**
 * @brief SpeakerPage::showDevice
 * 当无设备时，不显示设备信息
 * 当有多个设备，且未禁用时，显示设备信息
 * 跟随设备管理
 */
void SpeakerPage::showDevice()
{
    if (!m_speakSlider || !m_vbWidget || !m_balanceSlider || !m_outputSlider)
        return;

    if (1 > m_outputModel->rowCount()){
        setDeviceVisible(false);
        setBlueModeVisible(false);
    } else
        setDeviceVisible(true);
}

void SpeakerPage::setDeviceVisible(bool visible)
{
    if (visible) {
        m_speakSlider->show();
        m_vbWidget->show();
        if (GSettingWatcher::instance()->getStatus("soundBalanceSlider") != "Hidden")
            m_balanceSlider->setVisible(m_balance);
        if (GSettingWatcher::instance()->getStatus("soundOutputSlider") != "Hidden")
            m_outputSlider->show();
    } else {
        m_speakSlider->hide();
        m_vbWidget->hide();
        m_balanceSlider->hide();
        m_outputSlider->hide();
    }
}

void SpeakerPage::setBlueModeVisible(bool visible)
{
    m_blueSoundCbx->setVisible(visible);
}
