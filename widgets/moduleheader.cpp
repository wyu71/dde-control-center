#include <QLabel>
#include <QDebug>
#include <QTimer>
#include <QWidget>

#include <libdui/dthememanager.h>
#include <libdui/dtextbutton.h>

#include "moduleheader.h"

ModuleHeader::ModuleHeader(QString title, bool addResetButton, QWidget * parent) :
    DBaseLine(parent)
{
    D_THEME_INIT_WIDGET(ModuleHeader);

    m_title = new QLabel(title, this);
    this->setLeftContent(m_title);
    initUI(addResetButton);
}

ModuleHeader::ModuleHeader(QWidget *leftContent, bool addResetButton, QWidget *parent):
      DBaseLine(parent)
{
    D_THEME_INIT_WIDGET(ModuleHeader);

    this->setLeftContent(leftContent);
    initUI(addResetButton);
}
ModuleHeader::~ModuleHeader()
{
}
void ModuleHeader::initUI(bool addResetButton) {
    this->setFixedHeight(48);
    m_rightContentWidget = new RightContentWidget(addResetButton);

    this->setRightContent(m_rightContentWidget);
    connect(m_rightContentWidget, &RightContentWidget::resetButtonClicked,
            this, &ModuleHeader::resetButtonClicked);
}
RightContentWidget::RightContentWidget(bool addResetButton, QWidget *parent):
    QWidget(parent),
    m_timer(nullptr)
{
    if (addResetButton) {
        m_tipsLabel = new QLabel(tr("Reset Successfully"), this);
        m_tipsLabel->setObjectName("TipsLabel");
        m_reset = new DTextButton("Reset", this);
        m_timer = new QTimer;

        QHBoxLayout *layout = new QHBoxLayout;
        layout->addWidget(m_tipsLabel);
        layout->addWidget(m_reset);
        layout->setSpacing(0);
        layout->setMargin(0);
        setLayout(layout);
        m_timer->setInterval(800);

        connect(m_timer, &QTimer::timeout, this, &RightContentWidget::resetUI);
        connect(m_reset, &DTextButton::clicked, this, &RightContentWidget::resetButtonClicked);
        connect(m_reset, &DTextButton::clicked, [this] {
            m_tipsLabel->show();
            m_reset->hide();
            m_timer->start();
        });

        resetUI();
    }
}
void RightContentWidget::resetUI()
{
    m_tipsLabel->hide();
    m_reset->show();
}
RightContentWidget::~RightContentWidget()
{
    if (m_timer)
     m_timer->deleteLater();
}
