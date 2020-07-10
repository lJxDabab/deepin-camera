/*
* Copyright (C) 2020 ~ 2021 Uniontech Software Technology Co.,Ltd.
*
* Author:     shicetu <shicetu@uniontech.com>
*             hujianbo <hujianbo@uniontech.com>
* Maintainer: shicetu <shicetu@uniontech.com>
*             hujianbo <hujianbo@uniontech.com>
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

#include "mainwindow.h"

#include <QListWidgetItem>
#include <QTextLayout>
#include <QStyleFactory>
#include <QFileDialog>
#include <QStandardPaths>
#include <qsettingbackend.h>

#include <DLabel>
#include <DSettingsDialog>
#include <DSettingsOption>
#include <DSettings>
#include <DLineEdit>
#include <DFileDialog>
#include <DDialog>
#include <DDesktopServices>
#include <DMessageBox>
#include <dsettingswidgetfactory.h>

using namespace dc;

QString CMainWindow::m_lastfilename = {""};
static void workaround_updateStyle(QWidget *parent, const QString &theme)
{
    parent->setStyle(QStyleFactory::create(theme));
    for (auto obj : parent->children()) {
        auto w = qobject_cast<QWidget *>(obj);
        if (w) {
            workaround_updateStyle(w, theme);
        }
    }
}

static QString ElideText(const QString &text, const QSize &size,
                         QTextOption::WrapMode wordWrap, const QFont &font,
                         Qt::TextElideMode mode, int lineHeight, int lastLineWidth)
{
    int height = 0;

    QTextLayout textLayout(text);
    QString str = nullptr;
    QFontMetrics fontMetrics(font);

    textLayout.setFont(font);
    const_cast<QTextOption *>(&textLayout.textOption())->setWrapMode(wordWrap);

    textLayout.beginLayout();

    QTextLine line = textLayout.createLine();

    while (line.isValid()) {
        height += lineHeight;

        if (height + lineHeight >= size.height()) {
            str += fontMetrics.elidedText(text.mid(line.textStart() + line.textLength() + 1),
                                          mode, lastLineWidth);
            break;
        }

        line.setLineWidth(size.width());

        const QString &tmp_str = text.mid(line.textStart(), line.textLength());

        if (tmp_str.indexOf('\n'))
            height += lineHeight;

        str += tmp_str;

        line = textLayout.createLine();

        if (line.isValid())
            str.append("\n");
    }

    textLayout.endLayout();

    if (textLayout.lineCount() == 1) {
        str = fontMetrics.elidedText(str, mode, lastLineWidth);
    }

    return str;
}

static QWidget *createFormatLabelOptionHandle(QObject *opt)
{
    auto option = qobject_cast<DTK_CORE_NAMESPACE::DSettingsOption *>(opt);

    auto *lab = new DLabel();
    auto *main = new DWidget;
    auto *layout = new QHBoxLayout;

    main->setLayout(layout);
    main->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(lab);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->setAlignment(Qt::AlignVCenter);
    lab->setObjectName("OptionFormatLabel");
    lab->setFixedHeight(15);
    QString str = option->value().toString();
    lab->setText(option->value().toString());
    QFont font = lab->font();
    font.setPointSize(11);
    lab->setFont(font);
    lab->setAlignment(Qt::AlignVCenter);
    lab->show();
    //lab->setEnabled(false);
    auto optionWidget = DSettingsWidgetFactory::createTwoColumWidget(option, main);
    optionWidget->setContentsMargins(0, 0, 0, 0);
    workaround_updateStyle(optionWidget, "light");
    return optionWidget;
}

static QWidget *createSelectableLineEditOptionHandle(QObject *opt)
{
    auto option = qobject_cast<DTK_CORE_NAMESPACE::DSettingsOption *>(opt);

    auto le = new DLineEdit();
    auto main = new DWidget;
    auto layout = new QHBoxLayout;

    static QString nameLast = nullptr;

    main->setLayout(layout);
    auto *icon = new DPushButton;
    icon->setAutoDefault(false);
    le->setFixedHeight(30);
    le->setObjectName("OptionSelectableLineEdit");
    le->setText(option->value().toString());
    auto fm = le->fontMetrics();
    auto pe = ElideText(le->text(), {285, fm.height()}, QTextOption::WrapAnywhere,
                        le->font(), Qt::ElideMiddle, fm.height(), 285);
    option->connect(le, &DLineEdit::focusChanged, [ = ](bool on) {
        if (on)
            le->setText(option->value().toString());

    });
    le->setText(pe);
    nameLast = pe;
//    icon->setIconVisible(true);
    icon->setIcon(QIcon(":/images/icons/light/select-normal.svg"));
    icon->setIconSize(QSize(25, 25));
    icon->setFixedHeight(30);
    layout->addWidget(le);
    layout->addWidget(icon);

    auto optionWidget = DSettingsWidgetFactory::createTwoColumWidget(option, main);
    workaround_updateStyle(optionWidget, "light");

    DDialog *prompt = new DDialog(optionWidget);
    prompt->setIcon(QIcon(":/images/icons/warning.svg"));
    //prompt->setTitle(QObject::tr("Permissions prompt"));
    prompt->setMessage(QObject::tr("You don't have permission to operate this folder"));
    prompt->setWindowFlags(prompt->windowFlags() | Qt::WindowStaysOnTopHint);
    prompt->addButton(QObject::tr("Close"), true, DDialog::ButtonRecommend);

    auto validate = [ = ](QString name, bool alert = true) -> bool {
        name = name.trimmed();
        if (name.isEmpty()) return false;

        if (name.size() && name[0] == '~')
        {
            name.replace(0, 1, QDir::homePath());
        }

        QFileInfo fi(name);
        QDir dir(name);
        if (fi.exists())
        {
            if (!fi.isDir()) {
                if (alert) le->showAlertMessage(QObject::tr("Invalid folder"));
                return false;
            }

            if (!fi.isReadable() || !fi.isWritable()) {
//                if (alert) le->showAlertMessage(QObject::tr("You don't have permission to operate this folder"));
                return false;
            }
        } else
        {
            if (dir.cdUp()) {
                QFileInfo ch(dir.path());
                if (!ch.isReadable() || !ch.isWritable())
                    return false;
            }
        }

        return true;
    };

    option->connect(icon, &DPushButton::clicked, [ = ]() {
        QString name = DFileDialog::getExistingDirectory(nullptr, QObject::tr("Open folder"),
                                                         CMainWindow::lastOpenedPath(),
                                                         DFileDialog::ShowDirsOnly | DFileDialog::DontResolveSymlinks);
        if (validate(name, false)) {
            option->setValue(name);
            nameLast = name;
        }
        QFileInfo fm(name);
        if ((!fm.isReadable() || !fm.isWritable()) && !name.isEmpty()) {
            prompt->show();
        }
    });



    option->connect(le, &DLineEdit::editingFinished, option, [ = ]() {

        QString name = le->text();
        QDir dir(name);

        auto pn = ElideText(name, {285, fm.height()}, QTextOption::WrapAnywhere,
                            le->font(), Qt::ElideMiddle, fm.height(), 285);
        auto nmls = ElideText(nameLast, {285, fm.height()}, QTextOption::WrapAnywhere,
                              le->font(), Qt::ElideMiddle, fm.height(), 285);

        if (!validate(le->text(), false)) {
            QFileInfo fn(dir.path());
            if ((!fn.isReadable() || !fn.isWritable()) && !name.isEmpty()) {
                prompt->show();
            }
        }
        if (!le->lineEdit()->hasFocus()) {
            if (validate(le->text(), false)) {
                option->setValue(le->text());
                le->setText(pn);
                nameLast = name;
            } else if (pn == pe) {
                le->setText(pe);
            } else {
//                option->setValue(option->defaultValue());//设置为默认路径
//                le->setText(option->defaultValue().toString());
                option->setValue(nameLast);
                le->setText(nmls);
            }
        }
    });

    option->connect(le, &DLineEdit::textEdited, option, [ = ](const QString & newStr) {
        validate(newStr);
    });

    option->connect(option, &DTK_CORE_NAMESPACE::DSettingsOption::valueChanged, le,
    [ = ](const QVariant & value) {
        auto pi = ElideText(value.toString(), {285, fm.height()}, QTextOption::WrapAnywhere,
                            le->font(), Qt::ElideMiddle, fm.height(), 285);
        le->setText(pi);
        le->update();
    });

    return  optionWidget;
}

QString CMainWindow::lastOpenedPath()
{
    QString lastPath = Settings::get().generalOption("last_open_path").toString();
    QDir lastDir(lastPath);
    if (lastPath.isEmpty() || !lastDir.exists()) {
        lastPath = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
        QDir newLastDir(lastPath);
        if (!newLastDir.exists()) {
            lastPath = QDir::currentPath();
        }
    }

    return lastPath;
}

CMainWindow::CMainWindow(DWidget *w): DMainWindow (w)
{
    m_devnumMonitor = new DevNumMonitor();
    m_devnumMonitor->start();
    m_nActTpye = ActTakePic;
    initUI();
    initTitleBar();
    initConnection();
}

//DSettings *CMainWindow::getDsetMber()
//{
//    return pDSettings;
//}

CMainWindow::~CMainWindow()
{
}

void CMainWindow::slotPopupSettingsDialog()
{
    auto dsd = new DSettingsDialog(this);
    dsd->widgetFactory()->registerWidget("selectableEdit", createSelectableLineEditOptionHandle);
    dsd->widgetFactory()->registerWidget("formatLabel", createFormatLabelOptionHandle);

    connect(dsd, SIGNAL(destroyed()), this, SLOT(onSettingsDlgClose()));

    dsd->setProperty("_d_QSSThemename", "dark");
    dsd->setProperty("_d_QSSFilename", "DSettingsDialog");
    dsd->updateSettings(Settings::get().settings());

    auto reset = dsd->findChild<QPushButton *>("SettingsContentReset");
    reset->setDefault(false);
    reset->setAutoDefault(false);

    dsd->exec();
    delete dsd;
    Settings::get().settings()->sync();
}

void CMainWindow::initUI()
{
    this->setWindowFlag(Qt::FramelessWindowHint);
    m_closeDlg = new CloseDialog(this, tr("Video recording is in progress. Close the window?"));
    DWidget *wget = new DWidget;
    QVBoxLayout *hboxlayout = new QVBoxLayout;
    //创建设置存储后端，没有添加backend的时候，读取dsettings属性是json文件的
    //QSettingBackend *pBackend = new QSettingBackend(m_strCfgPath);
    //pDSettings->setBackend(pBackend);
//    g_lastFileName = pDSettings->value("base.save.datapath").toString();
//    if (g_lastFileName.size() && g_lastFileName[0] == '~') {
//        g_lastFileName.replace(0, 1, QDir::homePath());
//    }
//    m_pSettingDialog->setBackEndinIt(m_strCfgPath);
    CMainWindow::m_lastfilename = Settings::get().getOption("base.save.datapath").toString();
    if (CMainWindow::m_lastfilename.size() && CMainWindow::m_lastfilename[0] == '~') {
        CMainWindow::m_lastfilename.replace(0, 1, QDir::homePath());
    }
    m_videoPre.setSaveFolder(CMainWindow::m_lastfilename);
    if (QFileInfo(CMainWindow::m_lastfilename).exists()) {
        m_fileWatcher.addPath(CMainWindow::m_lastfilename);
    }
//    QPalette *pal = new QPalette(m_videoPre.palette());
//    pal->setColor(QPalette::Background, Qt::black); //设置黑色边框
//    m_videoPre.setAutoFillBackground(true);
//    m_videoPre.setPalette(*pal);
//    _animationlable = new AnimationLabel;
//    _animationlable->setAttribute(Qt::WA_TranslucentBackground);
//    _animationlable->setWindowFlags(Qt::FramelessWindowHint);
//    _animationlable->setParent(this);
//    _animationlable->setGeometry(width() / 2 - 100, height() / 2 - 100, 200, 200);

    //m_preWgt = new PreviewWidget(centralWidget());
    //hboxlayout->addWidget(&m_preWgt);
    hboxlayout->addWidget(&m_videoPre);
    hboxlayout->setContentsMargins(0, 0, 0, 0);

    wget->setLayout(hboxlayout);
    setCentralWidget(wget);

    setupTitlebar();

    m_thumbnail = new ThumbnailsBar(this);
    m_thumbnail->move(0, height() - 10);
    m_thumbnail->setFixedHeight(LAST_BUTTON_HEIGHT + LAST_BUTTON_SPACE * 2);

    //添加右键打开文件夹功能
    QMenu *menu = new QMenu();
    QAction *actOpen = new QAction(this);
    actOpen->setText(tr("Open folder"));
    menu->addAction(actOpen);
    m_thumbnail->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_thumbnail, &DLabel::customContextMenuRequested, this, [ = ](QPoint pos) {
        Q_UNUSED(pos);
        menu->exec(QCursor::pos());
    });
    connect(actOpen, &QAction::triggered, this, [ = ] {
        QString save_path = Settings::get().generalOption("last_open_path").toString();
        if (save_path.isEmpty())
        {
            save_path = Settings::get().getOption("base.save.datapath").toString();
        }
        if (save_path.size() && save_path[0] == '~')
        {
            save_path.replace(0, 1, QDir::homePath());
        }

        if (!QFileInfo(save_path).exists())
        {
            QDir d;
            d.mkpath(save_path);
        }
        Dtk::Widget::DDesktopServices::showFolder(save_path);
    });

    m_thumbnail->setVisible(true);
    m_thumbnail->setMaximumWidth(1200);
    m_thumbnail->m_nMaxItem = MinWindowWidth;
    m_thumbnail->addPath(CMainWindow::m_lastfilename);
    m_videoPre.setSaveFolder(CMainWindow::m_lastfilename);
    int nContinuous = Settings::get().getOption("photosetting.photosnumber.takephotos").toInt();
    int nDelayTime = Settings::get().getOption("photosetting.photosdelay.photodelays").toInt();
    switch (nContinuous) {
    case 1:
        nContinuous = 4;
        break;
    case 2:
        nContinuous = 10;
        break;
    default:
        nContinuous = 1;
        break;
    }
    switch (nDelayTime) {
    case 1:
        nDelayTime = 3;
        break;
    case 2:
        nDelayTime = 6;
        break;
    default:
        nDelayTime = 0;
        break;
    }
    m_videoPre.setInterval(nDelayTime);
    m_videoPre.setContinuous(nContinuous);
    this->resize(MinWindowWidth, MinWindowHeight);

}

void CMainWindow::initTitleBar()
{
    DGuiApplicationHelper::ColorType type = DGuiApplicationHelper::instance()->themeType();
    pDButtonBox = new DButtonBox();
    pDButtonBox->setFixedWidth(120);
    pDButtonBox->setFixedHeight(36);
    QList<DButtonBoxButton *> listButtonBox;
    QIcon iconPic(":/images/icons/light/button/photograph.svg");
    m_pTitlePicBtn = new DButtonBoxButton(nullptr/*iconPic*/);

    m_pTitlePicBtn->setIcon(iconPic);
    m_pTitlePicBtn->setIconSize(QSize(19, 16));

    DPalette pa = m_pTitlePicBtn->palette();
    QColor clo("#0081FF");
    pa.setColor(DPalette::Dark, clo);
    pa.setColor(DPalette::Light, clo);

    pa.setColor(DPalette::Button, clo);
    m_pTitlePicBtn->setPalette(pa);
    QIcon iconVd;

    if (type == DGuiApplicationHelper::UnknownType || type == DGuiApplicationHelper::LightType)
        iconVd = QIcon(":/images/icons/light/record video.svg");
    else
        iconVd = QIcon(":/images/icons/dark/button/record video_dark.svg");

    m_pTitleVdBtn = new DButtonBoxButton(nullptr);
    m_pTitleVdBtn->setIcon(iconVd);
    m_pTitleVdBtn->setIconSize(QSize(26, 16));

    listButtonBox.append(m_pTitlePicBtn);
    listButtonBox.append(m_pTitleVdBtn);
    pDButtonBox->setButtonList(listButtonBox, false);
    titlebar()->addWidget(pDButtonBox);


    pSelectBtn = new DIconButton(nullptr/*DStyle::SP_IndicatorSearch*/);
    if (type == DGuiApplicationHelper::UnknownType || type == DGuiApplicationHelper::LightType) {
        pSelectBtn->setIconSize(QSize(12, 12));
        pSelectBtn->setIcon(QIcon(":/images/icons/light/button/Switch camera.svg"));
    } else {
        pSelectBtn->setIconSize(QSize(24, 24));
        pSelectBtn->setIcon(QIcon(":/images/icons/dark/button/Switch camera_dark.svg"));
    }

    titlebar()->setIcon(QIcon::fromTheme(":/images/logo/deepin-camera-32px.svg"));// /usr/share/icons/bloom/apps/96 //preferences-system
    titlebar()->addWidget(pSelectBtn, Qt::AlignLeft);
}

void CMainWindow::initConnection()
{
    //connect(this, SIGNAL(windowstatechanged(Qt::WindowState windowState)), this, SLOT(onCapturepause(Qt::WindowState windowState)));
    //系统文件夹变化信号
    connect(&m_fileWatcher, SIGNAL(directoryChanged(const QString &)), m_thumbnail, SLOT(onFoldersChanged(const QString &)));
    //系统文件变化信号
    connect(&m_fileWatcher, SIGNAL(fileChanged(const QString &)), m_thumbnail, SLOT(onFoldersChanged(const QString &)));
    //增删文件修改界面
    connect(m_thumbnail, SIGNAL(fitToolBar()), this, SLOT(onFitToolBar()));

    //修改标题栏按钮状态
    connect(m_thumbnail, SIGNAL(enableTitleBar(int)), this, SLOT(onEnableTitleBar(int)));
    //录像信号
    connect(m_thumbnail, SIGNAL(takeVd()), &m_videoPre, SLOT(onTakeVideo()));
    //设置按钮信号
    connect(m_actionSettings, &QAction::triggered, this, &CMainWindow::slotPopupSettingsDialog);
    //禁用设置
    connect(m_thumbnail, SIGNAL(enableSettings(bool)), this, SLOT(onEnableSettings(bool)));
    //拍照信号--显示倒计时
    connect(m_thumbnail, SIGNAL(takePic()), &m_videoPre, SLOT(onTakePic()));

    //拍照结束
    connect(&m_videoPre, SIGNAL(takePicDone()), this, SLOT(onTakePicDone()));
    //录制取消 （倒计时3秒内的）
    connect(&m_videoPre, SIGNAL(takeVdCancel()), this, SLOT(onTakeVdCancel()));
    //设备切换信号
    connect(pSelectBtn, SIGNAL(clicked()), &m_videoPre, SLOT(changeDev()));
    //单设备信号
    connect(m_devnumMonitor, SIGNAL(seltBtnStateEnable()), this, SLOT(setSelBtnShow()));
    //多设备信号
    connect(m_devnumMonitor, SIGNAL(seltBtnStateDisable()), this, SLOT(setSelBtnHide()));

    connect(m_devnumMonitor, SIGNAL(existDevice()), &m_videoPre, SLOT(restartDevices()));

    //标题栏图片按钮
    connect(m_pTitlePicBtn, SIGNAL(clicked()), this, SLOT(onTitlePicBtn()));
    //标题栏视频按钮
    connect(m_pTitleVdBtn, SIGNAL(clicked()), this, SLOT(onTitleVdBtn()));
    //connect(m_closeDlg, SIGNAL(buttonClicked(int index, const QString & text)), this, SLOT(onCloseDlgBtnClicked(int index, const QString & text)));

    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, &CMainWindow::onThemeChange);
}
void CMainWindow::setSelBtnHide()
{
    pSelectBtn->hide();
}

void CMainWindow::setSelBtnShow()
{
    pSelectBtn->show();
}

void CMainWindow::setupTitlebar()
{
    menu = new DMenu();
    m_actionSettings = new QAction(tr("Settings"), this);
    menu->addAction(m_actionSettings);
    titlebar()->setMenu(menu);
}

void CMainWindow::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);

    int width = this->width();
    int height = this->height();
    if (m_thumbnail) {
        int n = m_thumbnail->getItemCount();
        int nWidth;
        if (n == 0) {
            nWidth = LAST_BUTTON_SPACE * 2 + LAST_BUTTON_WIDTH;
        } else {
            nWidth = n * THUMBNAIL_WIDTH + ITEM_SPACE * (n - 1) + LAST_BUTTON_SPACE * 4 + LAST_BUTTON_WIDTH + 4;
        }

        qDebug() << n << " " << nWidth;
        m_thumbnail->resize(/*qMin(width,TOOLBAR_MINIMUN_WIDTH)*/ nWidth, 90);

        m_thumbnail->move((width - m_thumbnail->width()) / 2,
                          height - m_thumbnail->height() - 5);
        m_thumbnail->m_nMaxItem = width;
    }

}

void CMainWindow::closeEvent(QCloseEvent *event)
{
    if (m_videoPre.getCapstatus()) {
        int ret = m_closeDlg->exec();
        switch (ret) {
        case 0:
            event->ignore();
            break;
        case 1:
            m_videoPre.endBtnClicked();
            event->accept();
            break;
        default:
            event->ignore();
            break;
        }
    }
}

void CMainWindow::changeEvent(QEvent *event)
{
    Q_UNUSED(event);
    if (this->windowState() == Qt::WindowMinimized) {
        set_capture_pause(1);
    } else if (this->isVisible() == true) {
        set_capture_pause(0);
    }
}

void CMainWindow::onFitToolBar()
{
    if (m_thumbnail) {
        int n = m_thumbnail->getItemCount();
        int nWidth = 0;
        if (n <= 0) {
            nWidth = LAST_BUTTON_SPACE * 2 + LAST_BUTTON_WIDTH;
        } else {
            nWidth = n * THUMBNAIL_WIDTH + ITEM_SPACE * (n - 1) + LAST_BUTTON_SPACE * 4 + LAST_BUTTON_WIDTH + 4;//4是选中边框宽度
        }

        m_thumbnail->resize(/*qMin(width,TOOLBAR_MINIMUN_WIDTH)*/ nWidth, THUMBNAIL_HEIGHT + 30);

        m_thumbnail->move((this->width() - m_thumbnail->width()) / 2,
                          this->height() - m_thumbnail->height() - 5);
    }
}

void CMainWindow::onEnableTitleBar(int nType)
{
    //1、禁用标题栏视频；2、禁用标题栏拍照；3、恢复标题栏视频；4、恢复标题栏拍照
    switch (nType) {
    case 1:
        m_pTitleVdBtn->setEnabled(false);
        break;
    case 2:
        m_pTitlePicBtn->setEnabled(false);
        break;
    case 3:
        m_pTitleVdBtn->setEnabled(true);
        break;
    case 4:
        m_pTitlePicBtn->setEnabled(true);
        break;
    default:
        break;
    }
}

void CMainWindow::menuItemInvoked(QAction *action)
{
    Q_UNUSED(action);
}

//void CMainWindow::keyPressEvent(QKeyEvent *ev)
//{
//    if (ev->key() == Qt::Key_Escape) {
//        m_setwidget->hide();
//    }
//    QWidget::keyReleaseEvent(ev);
//}

void CMainWindow::onTitlePicBtn()
{
    if (m_nActTpye == ActTakePic) {
        return;
    }
    int type = DGuiApplicationHelper::instance()->themeType();
    m_nActTpye = ActTakePic;
    //切换标题栏拍照按钮颜色
    DPalette pa = m_pTitlePicBtn->palette();
    QColor clo("#0081FF");
    pa.setColor(DPalette::Dark, clo);
    pa.setColor(DPalette::Light, clo);
    pa.setColor(DPalette::Button, clo);
    m_pTitlePicBtn->setPalette(pa);

    QIcon iconPic(":/images/icons/light/button/photograph.svg");
    m_pTitlePicBtn->setIcon(iconPic);

    //切换标题栏视频按钮颜色
    DPalette paVd = m_pTitleVdBtn->palette();
    QColor cloVd("#000000");
    cloVd.setAlpha(20);
    paVd.setColor(DPalette::Dark, cloVd);
    paVd.setColor(DPalette::Light, cloVd);
    paVd.setColor(DPalette::Button, cloVd);
    m_pTitleVdBtn->setPalette(paVd);
    if (type == DGuiApplicationHelper::UnknownType || type == DGuiApplicationHelper::LightType) {
        QIcon iconVd(":/images/icons/light/record video.svg");
        m_pTitleVdBtn->setIcon(iconVd);
    } else {
        QIcon iconVd(":/images/icons/dark/button/record video_dark.svg");
        m_pTitleVdBtn->setIcon(iconVd);
    }
    m_thumbnail->ChangeActType(m_nActTpye);
}

void CMainWindow::onTitleVdBtn()
{
    if (m_nActTpye == ActTakeVideo) {
        return;
    }
    DGuiApplicationHelper::ColorType type = DGuiApplicationHelper::instance()->themeType();
    m_nActTpye = ActTakeVideo;
    //切换标题栏视频按钮颜色
    DPalette paPic = m_pTitleVdBtn->palette();
    QColor cloPic("#0081FF");
    paPic.setColor(DPalette::Dark, cloPic);
    paPic.setColor(DPalette::Light, cloPic);
    paPic.setColor(DPalette::Button, cloPic);
    m_pTitleVdBtn->setPalette(paPic);

    QIcon iconVd(":/images/icons/light/button/transcribe.svg");
    m_pTitleVdBtn->setIcon(iconVd);

    //切换标题栏拍照按钮颜色
    DPalette paVd = m_pTitlePicBtn->palette();
    QColor cloVd("#000000");
    cloVd.setAlpha(20);
    paVd.setColor(DPalette::Dark, cloVd);
    paVd.setColor(DPalette::Light, cloVd);
    paVd.setColor(DPalette::Button, cloVd);
    m_pTitlePicBtn->setPalette(paVd);
    if (type == DGuiApplicationHelper::UnknownType || type == DGuiApplicationHelper::LightType) {
        QIcon iconVd(":/images/icons/light/photograph.svg");
        m_pTitlePicBtn->setIcon(iconVd);
    } else {
        QIcon iconPic(":/images/icons/dark/button/photograph_dark.svg");
        m_pTitlePicBtn->setIcon(iconPic);
    }
    m_thumbnail->ChangeActType(m_nActTpye);
}

void CMainWindow::onSettingsDlgClose()
{
//    if (settingDialog::getLastFileName().size() && settingDialog::getLastFileName()[0] == '~') {
//        settingDialog::getLastFileName().replace(0, 1, QDir::homePath());
//    }
//    m_fileWatcher.addPath(settingDialog::getLastFileName());
//    m_thumbnail->addPath(settingDialog::getLastFileName());
//    m_videoPre.setSaveFolder(settingDialog::getLastFileName());

//    int nContinuous = m_pSettingDialog->getValue("photosetting.photosnumber.takephotos").toInt();
//    int nDelayTime = m_pSettingDialog->getValue("photosetting.photosdelay.photodelays").toInt();

    /**********************************************/
    if (CMainWindow::m_lastfilename.size() && CMainWindow::m_lastfilename[0] == '~') {
        CMainWindow::m_lastfilename.replace(0, 1, QDir::homePath());
    }
    //m_fileWatcher.addPath(CMainWindow::m_lastfilename);
    //m_thumbnail->addPath(CMainWindow::m_lastfilename);
    m_videoPre.setSaveFolder(CMainWindow::m_lastfilename);

    int nContinuous = Settings::get().getOption("photosetting.photosnumber.takephotos").toInt();
    int nDelayTime = Settings::get().getOption("photosetting.photosdelay.photodelays").toInt();


    /**********************************************/
    switch (nContinuous) {
    case 1:
        nContinuous = 4;
        break;
    case 2:
        nContinuous = 10;
        break;
    default:
        nContinuous = 1;
        break;
    }
    switch (nDelayTime) {
    case 1:
        nDelayTime = 3;
        break;
    case 2:
        nDelayTime = 6;
        break;
    default:
        nDelayTime = 0;
        break;
    }
    m_videoPre.setInterval(nDelayTime);
    m_videoPre.setContinuous(nContinuous);
}

void CMainWindow::onEnableSettings(bool bTrue)
{
    m_actionSettings->setEnabled(bTrue);
}

void CMainWindow::onTakePicDone()
{
    onEnableTitleBar(3); //恢复按钮状态
    onEnableSettings(true);
    m_thumbnail->m_nStatus = STATNULL;
}

void CMainWindow::onTakeVdCancel() //保存视频完成，通过已有的文件检测实现缩略图恢复，这里不需要额外处理
{
    onEnableTitleBar(4); //恢复按钮状态
    m_thumbnail->m_nStatus = STATNULL;
    m_thumbnail->onFoldersChanged(""); //恢复缩略图
    m_thumbnail->show();
    onEnableSettings(true);
}

void CMainWindow::onThemeChange(DGuiApplicationHelper::ColorType type)
{
    if (type == DGuiApplicationHelper::UnknownType || type == DGuiApplicationHelper::LightType) {
        pSelectBtn->setIconSize(QSize(12, 12));
        pSelectBtn->setIcon(QIcon(":/images/icons/light/button/Switch camera.svg"));
        if (m_nActTpye == ActTakePic) {
            m_pTitleVdBtn->setIcon(QIcon(":/images/icons/light/record video.svg"));
        } else {
            m_pTitlePicBtn->setIcon(QIcon(":/images/icons/light/photograph.svg"));
        }
    }
    if (type == DGuiApplicationHelper::DarkType) {
        if (m_nActTpye == ActTakePic) {
            pSelectBtn->setIconSize(QSize(24, 24));
            pSelectBtn->setIcon(QIcon(":/images/icons/dark/button/Switch camera_dark.svg"));
            m_pTitleVdBtn->setIcon(QIcon(":/images/icons/dark/button/record video_dark.svg"));
        } else {
            m_pTitlePicBtn->setIcon(QIcon(":/images/icons/dark/button/photograph_dark.svg"));
        }
    }
}



//void CMainWindow::onCapturepause(Qt::WindowState windowState)
//{
//    if (windowState == Qt::WindowMinimized)
//        set_capture_pause(1);
//}
