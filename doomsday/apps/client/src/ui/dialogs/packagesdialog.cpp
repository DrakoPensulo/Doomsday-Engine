/** @file packagesdialog.cpp
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "ui/dialogs/packagesdialog.h"
#include "ui/widgets/packageswidget.h"
#include "ui/widgets/homeitemwidget.h"
#include "ui/widgets/homemenuwidget.h"
#include "ui/widgets/packagepopupwidget.h"
#include "clientapp.h"

#include <doomsday/Games>
#include <doomsday/LumpCatalog>

#include <de/CallbackAction>
#include <de/ChildWidgetOrganizer>
#include <de/DocumentPopupWidget>
#include <de/MenuWidget>
#include <de/NativeFile>
#include <de/PackageLoader>
#include <de/PopupButtonWidget>
#include <de/PopupMenuWidget>
#include <de/SequentialLayout>
#include <de/SignalAction>
#include <de/ui/SubwidgetItem>

using namespace de;

DENG_GUI_PIMPL(PackagesDialog)
, public ChildWidgetOrganizer::IWidgetFactory
, public PackagesWidget::IPackageStatus
, public PackagesWidget::IButtonHandler
, DENG2_OBSERVES(Widget, ChildAddition)
{
    StringList requiredPackages; // loaded first, cannot be changed
    StringList selectedPackages;
    LabelWidget *nothingSelected;
    HomeMenuWidget *menu;
    PackagesWidget *browser;
    LabelWidget *gameTitle;
    LabelWidget *gameDataFiles;
    Game const *game = nullptr;
    res::LumpCatalog catalog;

    /**
     * Information about a selected package. If the package file is not found, only
     * the ID is known.
     */
    class SelectedPackageItem : public ui::Item
    {
    public:
        SelectedPackageItem(String const &packageId)
        {
            setData(packageId);

            _file = App::packageLoader().select(packageId);
            if (_file)
            {
                _info = &_file->objectNamespace().subrecord(Package::VAR_PACKAGE);
            }
        }

        String packageId() const
        {
            return data().toString();
        }

        Record const *info() const
        {
            return _info;
        }

        File const *packageFile() const
        {
            return _file;
        }

    private:
        File   const *_file = nullptr;
        Record const *_info = nullptr;
    };

    /**
     * Widget showing information about a selected package, with a button for dragging
     * the item up and down.
     */
    class SelectedPackageWidget : public HomeItemWidget
    {
    public:
        SelectedPackageWidget(SelectedPackageItem const &item,
                              PackagesDialog &owner)
            : _owner(owner)
            , _item(&item)
        {
            useColorTheme(Normal, Normal);

            _removeButton = new ButtonWidget;
            _removeButton->setStyleImage("close.ringless", "small");
            _removeButton->margins().setTopBottom("unit");
            _removeButton->setActionFn([this] ()
            {
                _owner.d->removePackage(packageId());
                _owner.d->browser->updateItems();
            });
            addButton(_removeButton);
            setKeepButtonsVisible(true);

            // Package icon.
            icon().set(Background());
            icon().setImageFit(ui::FitToSize | ui::OriginalAspectRatio);
            icon().setStyleImage("package", "default");
            icon().margins().set("dialog.gap");
            Rule const &height = style().fonts().font("default").height();
            icon().rule().setInput(Rule::Width, height + rule("dialog.gap")*2);
        }

        void updateContents()
        {
            if (_item->info())
            {
                label().setText(_item->info()->gets("title"));
            }
            else
            {
                label().setText(_item->packageId());
            }
        }

        String packageId() const
        {
            return _item->packageId();
        }

        PopupWidget *makeInfoPopup() const
        {
            return new PackagePopupWidget(_item->packageFile());
        }

    private:
        PackagesDialog &_owner;
        SelectedPackageItem const *_item;
        ButtonWidget *_removeButton;
    };

    Instance(Public *i) : Base(i)
    {
        self.leftArea().add(gameTitle = new LabelWidget);
        gameTitle->add(gameDataFiles = new LabelWidget);

        // Indicator that is only visible when no packages have been added to the profile.
        nothingSelected = new LabelWidget;
        nothingSelected->setText(_E(b) + tr("Nothing Selected"));
        nothingSelected->setFont("heading");
        nothingSelected->setOpacity(0.5f);
        nothingSelected->rule()
                .setRect(self.leftArea().rule())
                .setInput(Rule::Top, gameTitle->rule().bottom());
        self.leftArea().add(nothingSelected);

        // Currently selected packages.
        gameTitle->setSizePolicy(ui::Filled, ui::Expand);
        gameTitle->setImageFit(ui::FitToWidth | ui::OriginalAspectRatio | ui::CoverArea);
        gameTitle->margins().setZero();
        gameDataFiles->setFont("small");
        gameDataFiles->setSizePolicy(ui::Fixed, ui::Expand);
        gameDataFiles->set(Background(style().colors().colorf("background")));
        gameDataFiles->setTextLineAlignment(ui::AlignLeft);
        gameDataFiles->setAlignment(ui::AlignLeft);
        gameTitle->rule()
                .setInput(Rule::Left,  self.leftArea().contentRule().left())
                .setInput(Rule::Top,   self.leftArea().contentRule().top())
                .setInput(Rule::Width, rule("dialog.packages.width"));
        gameDataFiles->rule()
                .setRect(gameTitle->rule())
                .clearInput(Rule::Top);
        self.leftArea().add(menu = new HomeMenuWidget);
        menu->layout().setRowPadding(Const(0));
        menu->rule()
                .setInput(Rule::Left,  self.leftArea().contentRule().left())
                .setInput(Rule::Top,   gameTitle->rule().bottom())
                .setInput(Rule::Width, rule("dialog.packages.width"));
        menu->organizer().setWidgetFactory(*this);
        menu->audienceForChildAddition() += this;
        self.leftArea().enableIndicatorDraw(true);

        QObject::connect(menu, &HomeMenuWidget::itemClicked, [this] (int index)
        {
            if (index >= 0)
            {
                browser->scrollToPackage(menu->items().at(index)
                                         .as<SelectedPackageItem>().packageId());
            }
        });

        // Package browser.
        self.rightArea().add(browser = new PackagesWidget);
        browser->setActionButtonAlwaysShown(true);
        browser->setPackageStatus(*this);
        browser->setButtonHandler(*this);
        //browser->setButtonLabels(tr("Add"), tr("Remove"));
        browser->setButtonLabels("", "");
        browser->setButtonImages("create", "close.ringless");
        //browser->setColorTheme(Normal, Normal, Normal, Normal);
        browser->rule()
                .setInput(Rule::Left,  self.rightArea().contentRule().left())
                .setInput(Rule::Top,   self.rightArea().contentRule().top())
                .setInput(Rule::Width, menu->rule().width());
        self.rightArea().enableIndicatorDraw(true);
    }

    ~Instance()
    {
        menu->audienceForChildAddition() -= this;
    }

    void populate()
    {
        menu->items().clear();

        // Remove from the list those packages that are no longer listed.
        for (String packageId : selectedPackages)
        {
            menu->items() << new SelectedPackageItem(packageId);
        }

        updateNothingIndicator();
    }

    void updateNothingIndicator()
    {
        nothingSelected->setOpacity(menu->items().isEmpty()? .5f : 0.f, 0.4);
    }

    void updateGameTitle()
    {
        if (game && catalog.setPackages(requiredPackages + selectedPackages))
        {
            gameTitle->setImage(HomeItemWidget::makeGameLogo(*game, catalog,
                                                             HomeItemWidget::UnmodifiedAppearance));
            // List of the native required files.
            StringList dataFiles;
            for (String packageId : requiredPackages)
            {
                if (File const *file = App::packageLoader().select(packageId))
                {
                    // Only list here the game data files; Doomsday's PK3s are always
                    // there so listing them is not very helpful.
                    if (Package::tags(*file).contains(QStringLiteral("gamedata")))
                    {
                        // Resolve indirection (symbolic links and interpretations) to
                        // describe the actual source file of the package.
                        dataFiles << file->source()->description(0);
                    }
                }
            }
            gameDataFiles->setText(_E(l) + String::format("Data file%s: ", dataFiles.size() != 1? "s" : "") +
                                   _E(.) + String::join(dataFiles, _E(l) " and " _E(.)));
        }
    }

    GuiWidget *makeItemWidget(ui::Item const &item, GuiWidget const *)
    {
        return new SelectedPackageWidget(item.as<SelectedPackageItem>(), self);
    }

    void updateItemWidget(GuiWidget &widget, ui::Item const &)
    {
        widget.as<SelectedPackageWidget>().updateContents();
    }

    bool isPackageHighlighted(String const &packageId) const override
    {
        return selectedPackages.contains(packageId);
    }

    void packageButtonClicked(ButtonWidget &, String const &packageId) override
    {
        if (!selectedPackages.contains(packageId))
        {
            selectedPackages.append(packageId);
            menu->items() << new SelectedPackageItem(packageId);
            updateNothingIndicator();
        }
        else
        {
            removePackage(packageId);
        }
    }

    void removePackage(String const &packageId)
    {
        selectedPackages.removeOne(packageId);
        auto pos = menu->items().findData(packageId);
        DENG2_ASSERT(pos != ui::Data::InvalidPos);
        menu->items().remove(pos);
        updateNothingIndicator();
    }

    void widgetChildAdded(Widget &child)
    {
        ui::DataPos pos = menu->findItem(child.as<GuiWidget>());
        // We use a delay here because ScrollAreaWidget does scrolling based on
        // the current geometry of the widget and HomeItemWidget uses an animation
        // for its height.
        Loop::get().timer(0.3, [this, pos] ()
        {
            menu->setSelectedIndex(pos);
        });
    }
};

PackagesDialog::PackagesDialog(String const &titleText)
    : DialogWidget("packages", WithHeading)
    , d(new Instance(this))
{
    if (titleText.isEmpty())
    {
        heading().setText(tr("Packages"));
    }
    else
    {
        heading().setText(tr("Packages: %1").arg(titleText));
    }
    heading().setImage(style().images().image("package"));
    buttons()
            << new DialogButtonItem(Default | Accept, tr("OK"))
            << new DialogButtonItem(Reject, tr("Cancel"))
            << new DialogButtonItem(Action, style().images().image("refresh"),
                                    new SignalAction(this, SLOT(refreshPackages())));

    // The individual menus will be scrolling independently.
    leftArea() .setContentSize(d->menu->rule().width(),
                               OperatorRule::maximum(d->menu->rule().height(),
                                                     d->nothingSelected->rule().height()) +
                               d->gameTitle->rule().height());
    rightArea().setContentSize(d->browser->rule().width(), d->browser->rule().height());

    refreshPackages();
}

void PackagesDialog::setGame(String const &gameId)
{
    d->game = &DoomsdayApp::games()[gameId];
    d->requiredPackages = d->game->requiredPackages();
    d->updateGameTitle();
}

void PackagesDialog::setSelectedPackages(StringList const &packages)
{
    d->selectedPackages = packages;
    d->browser->populate();
    d->updateGameTitle();
}

StringList PackagesDialog::selectedPackages() const
{
    return d->selectedPackages;
}

void PackagesDialog::refreshPackages()
{
    App::fileSystem().refresh();
    d->populate();
}

void PackagesDialog::preparePanelForOpening()
{
    DialogWidget::preparePanelForOpening();
    d->populate();

    root().setFocus(&d->browser->searchTermsEditor());
}
