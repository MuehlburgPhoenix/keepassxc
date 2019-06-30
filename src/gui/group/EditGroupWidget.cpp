/*
 *  Copyright (C) 2011 Felix Geyer <debfx@fobos.de>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "EditGroupWidget.h"
#include "ui_EditGroupWidgetMain.h"

#include "core/FilePath.h"
#include "core/Metadata.h"
#include "gui/EditWidgetIcons.h"
#include "gui/EditWidgetProperties.h"
#include "gui/MessageBox.h"

#if defined(WITH_XC_KEESHARE)
#include "keeshare/group/EditGroupPageKeeShare.h"
#endif

class EditGroupWidget::ExtraPage
{
public:
    ExtraPage(IEditGroupPage* page, QWidget* widget)
        : editPage(page)
        , widget(widget)
    {
    }

    void set(Group* temporaryGroup, QSharedPointer<Database> database) const
    {
        editPage->set(widget, temporaryGroup, database);
    }

    void assign() const
    {
        editPage->assign(widget);
    }

    QWidget* getWidget()
    {
        return widget;
    }

private:
    QSharedPointer<IEditGroupPage> editPage;
    QWidget* widget;
};

EditGroupWidget::EditGroupWidget(QWidget* parent)
    : EditWidget(parent)
    , m_mainUi(new Ui::EditGroupWidgetMain())
    , m_editGroupWidgetMain(new QWidget())
    , m_editGroupWidgetIcons(new EditWidgetIcons())
    , m_editWidgetProperties(new EditWidgetProperties())
    , m_group(nullptr)
{
    m_mainUi->setupUi(m_editGroupWidgetMain);

    addPage(tr("Group"), FilePath::instance()->icon("actions", "document-edit"), m_editGroupWidgetMain);
    addPage(tr("Icon"), FilePath::instance()->icon("apps", "preferences-desktop-icons"), m_editGroupWidgetIcons);
#if defined(WITH_XC_KEESHARE)
    addEditPage(new EditGroupPageKeeShare(this));
#endif
    addPage(tr("Properties"), FilePath::instance()->icon("actions", "document-properties"), m_editWidgetProperties);

    connect(m_mainUi->expireCheck, SIGNAL(toggled(bool)), m_mainUi->expireDatePicker, SLOT(setEnabled(bool)));
    connect(m_mainUi->autoTypeSequenceCustomRadio,
            SIGNAL(toggled(bool)),
            m_mainUi->autoTypeSequenceCustomEdit,
            SLOT(setEnabled(bool)));

    connect(this, SIGNAL(apply()), SLOT(apply()));
    connect(this, SIGNAL(accepted()), SLOT(save()));
    connect(this, SIGNAL(rejected()), SLOT(cancel()));

    // clang-format off
    connect(m_editGroupWidgetIcons,
            SIGNAL(messageEditEntry(QString,MessageWidget::MessageType)),
            SLOT(showMessage(QString,MessageWidget::MessageType)));
    // clang-format on

    connect(m_editGroupWidgetIcons, SIGNAL(messageEditEntryDismiss()), SLOT(hideMessage()));

    setupModifiedTracking();

    m_mainUi->defaultExpirationPeriodPresetsButton->setMenu(createDefaultExpirationPeriodPresetsMenu());
}

EditGroupWidget::~EditGroupWidget()
{
}

void EditGroupWidget::setupModifiedTracking()
{
    // Group tab
    connect(m_mainUi->editName, SIGNAL(textChanged(QString)), SLOT(setModified()));
    connect(m_mainUi->editNotes, SIGNAL(textChanged()), SLOT(setModified()));
    connect(m_mainUi->expireCheck, SIGNAL(stateChanged(int)), SLOT(setModified()));
    connect(m_mainUi->expireDatePicker, SIGNAL(dateTimeChanged(QDateTime)), SLOT(setModified()));
    connect(m_mainUi->defaultExpirationPeriodComboBox, SIGNAL(currentIndexChanged(int)), SLOT(setModified()));
    connect(m_mainUi->defaultExpirationPeriodComboBox,
            SIGNAL(currentIndexChanged(int)),
            SLOT(updateExpirationPeriodWidgets(int)));
    connect(m_mainUi->searchComboBox, SIGNAL(currentIndexChanged(int)), SLOT(setModified()));
    connect(m_mainUi->autotypeComboBox, SIGNAL(currentIndexChanged(int)), SLOT(setModified()));
    connect(m_mainUi->autoTypeSequenceInherit, SIGNAL(toggled(bool)), SLOT(setModified()));
    connect(m_mainUi->autoTypeSequenceCustomRadio, SIGNAL(toggled(bool)), SLOT(setModified()));
    connect(m_mainUi->autoTypeSequenceCustomEdit, SIGNAL(textChanged(QString)), SLOT(setModified()));

    // Icon tab
    connect(m_editGroupWidgetIcons, SIGNAL(widgetUpdated()), SLOT(setModified()));
}

QMenu* EditGroupWidget::createDefaultExpirationPeriodPresetsMenu()
{
    auto* expirePresetsMenu = new QMenu(this);
    expirePresetsMenu->addAction(tr("%n day(s)", nullptr, 1))->setData(QVariant::fromValue(TimeDelta::fromDays(1)));
    expirePresetsMenu->addSeparator();
    expirePresetsMenu->addAction(tr("%n week(s)", nullptr, 1))->setData(QVariant::fromValue(TimeDelta::fromDays(7)));
    expirePresetsMenu->addAction(tr("%n week(s)", nullptr, 2))->setData(QVariant::fromValue(TimeDelta::fromDays(14)));
    expirePresetsMenu->addAction(tr("%n week(s)", nullptr, 3))->setData(QVariant::fromValue(TimeDelta::fromDays(21)));
    expirePresetsMenu->addSeparator();
    expirePresetsMenu->addAction(tr("%n month(s)", nullptr, 1))->setData(QVariant::fromValue(TimeDelta::fromMonths(1)));
    expirePresetsMenu->addAction(tr("%n month(s)", nullptr, 3))->setData(QVariant::fromValue(TimeDelta::fromMonths(3)));
    expirePresetsMenu->addAction(tr("%n month(s)", nullptr, 6))->setData(QVariant::fromValue(TimeDelta::fromMonths(6)));
    expirePresetsMenu->addSeparator();
    expirePresetsMenu->addAction(tr("%n year(s)", nullptr, 1))->setData(QVariant::fromValue(TimeDelta::fromYears(1)));
    expirePresetsMenu->addAction(tr("%n year(s)", nullptr, 2))->setData(QVariant::fromValue(TimeDelta::fromYears(2)));
    expirePresetsMenu->addAction(tr("%n year(s)", nullptr, 3))->setData(QVariant::fromValue(TimeDelta::fromYears(3)));
    return expirePresetsMenu;
}

void EditGroupWidget::loadGroup(Group* group, bool create, const QSharedPointer<Database>& database)
{
    m_group = group;
    m_db = database;

    m_temporaryGroup.reset(group->clone(Entry::CloneNoFlags, Group::CloneNoFlags));
    connect(m_temporaryGroup->customData(), SIGNAL(customDataModified()), SLOT(setModified()));

    if (create) {
        setHeadline(tr("Add group"));
    } else {
        setHeadline(tr("Edit group"));
    }

    if (m_group->parentGroup()) {
        addTriStateItems(m_mainUi->searchComboBox, m_group->parentGroup()->resolveSearchingEnabled());
        addTriStateItems(m_mainUi->autotypeComboBox, m_group->parentGroup()->resolveAutoTypeEnabled());
        addTriStateItems(m_mainUi->defaultExpirationPeriodComboBox,
                         m_group->parentGroup()->resolveDefaultExpirationPeriodEnabled());
    } else {
        addTriStateItems(m_mainUi->searchComboBox, true);
        addTriStateItems(m_mainUi->autotypeComboBox, true);
        addTriStateItems(m_mainUi->defaultExpirationPeriodComboBox, false);
    }

    m_mainUi->editName->setText(m_group->name());
    m_mainUi->editNotes->setPlainText(m_group->notes());
    m_mainUi->expireCheck->setChecked(group->timeInfo().expires());
    m_mainUi->expireDatePicker->setDateTime(group->timeInfo().expiryTime().toLocalTime());
    m_mainUi->defaultExpirationPeriodYearsSpinBox->setValue(group->defaultExpirationPeriod().getYears());
    m_mainUi->defaultExpirationPeriodMonthsSpinBox->setValue(group->defaultExpirationPeriod().getMonths());
    m_mainUi->defaultExpirationPeriodDaysSpinBox->setValue(group->defaultExpirationPeriod().getDays());
    m_mainUi->defaultExpirationPeriodComboBox->setCurrentIndex(
        indexFromTriState(group->defaultExpirationPeriodEnabled()));
    updateExpirationPeriodWidgets(m_mainUi->defaultExpirationPeriodComboBox->currentIndex());
    m_mainUi->searchComboBox->setCurrentIndex(indexFromTriState(group->searchingEnabled()));
    m_mainUi->autotypeComboBox->setCurrentIndex(indexFromTriState(group->autoTypeEnabled()));
    if (group->defaultAutoTypeSequence().isEmpty()) {
        m_mainUi->autoTypeSequenceInherit->setChecked(true);
    } else {
        m_mainUi->autoTypeSequenceCustomRadio->setChecked(true);
    }
    m_mainUi->autoTypeSequenceCustomEdit->setText(group->effectiveAutoTypeSequence());

    IconStruct iconStruct;
    iconStruct.uuid = m_temporaryGroup->iconUuid();
    iconStruct.number = m_temporaryGroup->iconNumber();
    m_editGroupWidgetIcons->load(m_temporaryGroup->uuid(), m_db, iconStruct);
    m_editWidgetProperties->setFields(m_temporaryGroup->timeInfo(), m_temporaryGroup->uuid());
    m_editWidgetProperties->setCustomData(m_temporaryGroup->customData());

    for (const ExtraPage& page : asConst(m_extraPages)) {
        page.set(m_temporaryGroup.data(), m_db);
    }

    setCurrentPage(0);

    m_mainUi->editName->setFocus();

    // Force the user to Save/Discard new groups
    showApplyButton(!create);

    setModified(false);
}

void EditGroupWidget::updateExpirationPeriodWidgets(int index)
{
    Group::TriState triState;
    if (index >= 0) {
        triState = triStateFromIndex(index);
    } else {
        triState = Group::Disable;
    }

    switch (triState) {
    case Group::Enable: {
        m_mainUi->defaultExpirationPeriodYearsSpinBox->setEnabled(true);
        m_mainUi->defaultExpirationPeriodMonthsSpinBox->setEnabled(true);
        m_mainUi->defaultExpirationPeriodDaysSpinBox->setEnabled(true);
        m_mainUi->defaultExpirationPeriodPresetsButton->setEnabled(true);
        break;
    }
    case Group::Inherit: {
        TimeDelta inheritedPeriod = m_group->effectiveDefaultExpirationPeriod();
        m_mainUi->defaultExpirationPeriodYearsSpinBox->setValue(inheritedPeriod.getYears());
        m_mainUi->defaultExpirationPeriodMonthsSpinBox->setValue(inheritedPeriod.getMonths());
        m_mainUi->defaultExpirationPeriodDaysSpinBox->setValue(inheritedPeriod.getDays());
        [[fallthrough]];
    }
    case Group::Disable: {
        m_mainUi->defaultExpirationPeriodYearsSpinBox->setEnabled(false);
        m_mainUi->defaultExpirationPeriodMonthsSpinBox->setEnabled(false);
        m_mainUi->defaultExpirationPeriodDaysSpinBox->setEnabled(false);
        m_mainUi->defaultExpirationPeriodPresetsButton->setEnabled(false);
        break;
    }
    default:
        Q_ASSERT(false);
        break;
    }
}

void EditGroupWidget::save()
{
    apply();
    clear();
    emit editFinished(true);
}

void EditGroupWidget::apply()
{
    m_temporaryGroup->setName(m_mainUi->editName->text());
    m_temporaryGroup->setNotes(m_mainUi->editNotes->toPlainText());
    m_temporaryGroup->setExpires(m_mainUi->expireCheck->isChecked());
    m_temporaryGroup->setExpiryTime(m_mainUi->expireDatePicker->dateTime().toUTC());

    Group::TriState defaultExpirationPeriodEnabled =
        triStateFromIndex(m_mainUi->defaultExpirationPeriodComboBox->currentIndex());
    m_temporaryGroup->setDefaultExpirationPeriodEnabled(defaultExpirationPeriodEnabled);
    if (defaultExpirationPeriodEnabled == Group::Enable) {
        TimeDelta defaultExpirationPeriod(m_mainUi->defaultExpirationPeriodDaysSpinBox->value(),
                                          m_mainUi->defaultExpirationPeriodMonthsSpinBox->value(),
                                          m_mainUi->defaultExpirationPeriodYearsSpinBox->value());
        m_temporaryGroup->setDefaultExpirationPeriod(defaultExpirationPeriod);
    }

    m_temporaryGroup->setSearchingEnabled(triStateFromIndex(m_mainUi->searchComboBox->currentIndex()));
    m_temporaryGroup->setAutoTypeEnabled(triStateFromIndex(m_mainUi->autotypeComboBox->currentIndex()));

    if (m_mainUi->autoTypeSequenceInherit->isChecked()) {
        m_temporaryGroup->setDefaultAutoTypeSequence(QString());
    } else {
        m_temporaryGroup->setDefaultAutoTypeSequence(m_mainUi->autoTypeSequenceCustomEdit->text());
    }

    IconStruct iconStruct = m_editGroupWidgetIcons->state();

    if (iconStruct.number < 0) {
        m_temporaryGroup->setIcon(Group::DefaultIconNumber);
    } else if (iconStruct.uuid.isNull()) {
        m_temporaryGroup->setIcon(iconStruct.number);
    } else {
        m_temporaryGroup->setIcon(iconStruct.uuid);
    }

    for (const ExtraPage& page : asConst(m_extraPages)) {
        page.assign();
    }

    // Icons add/remove are applied globally outside the transaction!
    m_group->copyDataFrom(m_temporaryGroup.data());

    // Assign the icon to children if selected
    if (iconStruct.applyTo == ApplyIconToOptions::CHILD_GROUPS
        || iconStruct.applyTo == ApplyIconToOptions::ALL_CHILDREN) {
        m_group->applyGroupIconToChildGroups();
    }

    if (iconStruct.applyTo == ApplyIconToOptions::CHILD_ENTRIES
        || iconStruct.applyTo == ApplyIconToOptions::ALL_CHILDREN) {
        m_group->applyGroupIconToChildEntries();
    }

    setModified(false);
}

void EditGroupWidget::cancel()
{
    if (!m_group->iconUuid().isNull() && !m_db->metadata()->containsCustomIcon(m_group->iconUuid())) {
        m_group->setIcon(Entry::DefaultIconNumber);
    }

    if (isModified()) {
        auto result = MessageBox::question(this,
                                           QString(),
                                           tr("Entry has unsaved changes"),
                                           MessageBox::Cancel | MessageBox::Save | MessageBox::Discard,
                                           MessageBox::Cancel);
        if (result == MessageBox::Cancel) {
            return;
        }
        if (result == MessageBox::Save) {
            apply();
            setModified(false);
        }
    }

    clear();
    emit editFinished(false);
}

void EditGroupWidget::clear()
{
    m_group = nullptr;
    m_db.reset();
    m_temporaryGroup.reset(nullptr);
    m_editGroupWidgetIcons->reset();
}

void EditGroupWidget::addEditPage(IEditGroupPage* page)
{
    QWidget* widget = page->createWidget();
    widget->setParent(this);

    m_extraPages.append(ExtraPage(page, widget));
    addPage(page->name(), page->icon(), widget);
}

void EditGroupWidget::addTriStateItems(QComboBox* comboBox, bool inheritDefault)
{
    QString inheritDefaultString;
    if (inheritDefault) {
        inheritDefaultString = tr("Enable");
    } else {
        inheritDefaultString = tr("Disable");
    }

    comboBox->clear();
    comboBox->addItem(tr("Inherit from parent group (%1)").arg(inheritDefaultString));
    comboBox->addItem(tr("Enable"));
    comboBox->addItem(tr("Disable"));
}

int EditGroupWidget::indexFromTriState(Group::TriState triState)
{
    switch (triState) {
    case Group::Inherit:
        return 0;
    case Group::Enable:
        return 1;
    case Group::Disable:
        return 2;
    default:
        Q_ASSERT(false);
        return 0;
    }
}

Group::TriState EditGroupWidget::triStateFromIndex(int index)
{
    switch (index) {
    case 0:
        return Group::Inherit;
    case 1:
        return Group::Enable;
    case 2:
        return Group::Disable;
    default:
        Q_ASSERT(false);
        return Group::Inherit;
    }
}
