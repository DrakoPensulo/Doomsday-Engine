/** @file packageswidget.cpp  Widget for searching and examining packages.
 *
 * @authors Copyright (c) 2015-2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/home/packageswidget.h"
#include "clientapp.h"

#include <de/FileSystem>
#include <de/MenuWidget>
#include <de/LineEditWidget>
#include <de/ChildWidgetOrganizer>
#include <de/SequentialLayout>
#include <de/DocumentPopupWidget>
#include <de/PopupButtonWidget>
#include <de/SignalAction>
#include <de/CallbackAction>

using namespace de;

DENG_GUI_PIMPL(PackagesWidget)
, public ChildWidgetOrganizer::IWidgetFactory
{
    LineEditWidget *search;
    MenuWidget *menu;

    /**
     * Information about an available package.
     */
    struct PackageItem : public ui::Item
    {
        File const *file;
        Record const *info;

        PackageItem(File const &packFile)
            : file(&packFile)
            , info(&file->objectNamespace().subrecord(Package::VAR_PACKAGE))
        {
            setData(QString(info->gets("ID")));
        }

        void setFile(File const &packFile)
        {
            file = &packFile;
            info = &file->objectNamespace().subrecord(Package::VAR_PACKAGE);
            notifyChange();
        }
    };

    /**
     * Widget showing information about a package and containing buttons for manipulating
     * the package.
     */
    class ItemWidget : public GuiWidget
    {
    private:
        /// Action to load or unload a package.
        struct LoadAction : public Action
        {
            ItemWidget &owner;

            LoadAction(ItemWidget &widget) : owner(widget) {}

            void trigger()
            {
                Action::trigger();
                auto &loader = App::packageLoader();
                if(loader.isLoaded(owner.packageId()))
                {
                    loader.unload(owner.packageId());
                }
                else
                {
                    try
                    {
                        loader.load(owner.packageId());
                    }
                    catch(Error const &er)
                    {
                        LOG_RES_ERROR("Package \"" + owner.packageId() +
                                      "\" could not be loaded: " + er.asText());
                    }
                }
                owner.updateContents();
            }
        };

    public:
        ItemWidget(PackageItem const &item)
            : _item(&item)
        {
            add(_title = new LabelWidget);
            _title->setSizePolicy(ui::Fixed, ui::Expand);
            _title->setAlignment(ui::AlignLeft);
            _title->setTextLineAlignment(ui::AlignLeft);
            _title->margins().setBottom("").setLeft("unit");

            add(_subtitle = new LabelWidget);
            _subtitle->setSizePolicy(ui::Fixed, ui::Expand);
            _subtitle->setAlignment(ui::AlignLeft);
            _subtitle->setTextLineAlignment(ui::AlignLeft);
            _subtitle->setFont("small");
            _subtitle->setTextColor("accent");
            _subtitle->margins().setTop("").setBottom("unit").setLeft("unit");

            add(_loadButton = new ButtonWidget);
            _loadButton->setSizePolicy(ui::Expand, ui::Expand);
            _loadButton->setAction(new LoadAction(*this));

            add(_infoButton = new PopupButtonWidget);
            _infoButton->setSizePolicy(ui::Expand, ui::Fixed);
            _infoButton->setText(_E(s)_E(B) + tr("..."));
            _infoButton->setPopup([this] (PopupButtonWidget const &) {
                return makeInfoPopup();
            }, ui::Left);

            createTagButtons();

            AutoRef<Rule> titleWidth(rule().width() -
                                     _loadButton->rule().width() -
                                     _infoButton->rule().width());

            _title->rule()
                    .setInput(Rule::Width, titleWidth)
                    .setInput(Rule::Left,  rule().left())
                    .setInput(Rule::Top,   rule().top());
            _subtitle->rule()
                    .setInput(Rule::Width, titleWidth)
                    .setInput(Rule::Left,  rule().left())
                    .setInput(Rule::Top,   _title->rule().bottom());

            _loadButton->rule()
                    .setInput(Rule::Right, rule().right() - _infoButton->rule().width())
                    .setMidAnchorY(rule().midY());
            _infoButton->rule()
                    .setInput(Rule::Right,  rule().right())
                    .setInput(Rule::Height, _loadButton->rule().height())
                    .setMidAnchorY(rule().midY());

            rule().setInput(Rule::Width,  style().rules().rule("dialog.packages.width"))
                  .setInput(Rule::Height, _title->rule().height() +
                            _subtitle->rule().height() + _tags.at(0)->rule().height());
        }

        void createTagButtons()
        {
            SequentialLayout layout(_subtitle->rule().left(),
                                    _subtitle->rule().bottom(), ui::Right);

            for(QString tag : Package::tags(*_item->file))
            {
                auto *btn = new ButtonWidget;
                btn->setText(_E(l) + tag.toLower());
                updateTagButtonStyle(btn, "accent");
                btn->setSizePolicy(ui::Expand, ui::Expand);
                btn->margins()
                        .setTop("unit").setBottom("unit")
                        .setLeft("gap").setRight("gap");
                add(btn);

                layout << *btn;
                _tags.append(btn);
            }
        }

        void updateTagButtonStyle(ButtonWidget *tag, String const &color)
        {
            tag->setFont("small");
            tag->setTextColor(color);
            tag->set(Background(Background::Rounded, style().colors().colorf(color), 6));
        }

        void updateContents()
        {
            _title->setText(_item->info->gets("title"));
            _subtitle->setText(packageId());

            String auxColor = "accent";

            if(isLoaded())
            {
                _loadButton->setText(tr("Unload"));
                _loadButton->setTextColor("altaccent");
                _loadButton->setBorderColor("altaccent");
                _title->setFont("choice.selected");
                auxColor = "altaccent";
            }
            else
            {
                _loadButton->setText(tr("Load"));
                _loadButton->setTextColor("text");
                _loadButton->setBorderColor("text");
                _title->setFont("default");
            }

            _subtitle->setTextColor(auxColor);
            for(ButtonWidget *b : _tags)
            {
                updateTagButtonStyle(b, auxColor);
            }
        }

        bool isLoaded() const
        {
            return App::packageLoader().isLoaded(packageId());
        }

        String packageId() const
        {
            return _item->info->gets("ID");
        }

        PopupWidget *makeInfoPopup() const
        {
            auto *pop = new DocumentPopupWidget;
            pop->document().setText(QString(_E(1) "%1" _E(.) "\n%2\n"
                                            _E(l) "Version: " _E(.) "%3\n"
                                            _E(l) "License: " _E(.)_E(>) "%4" _E(<)
                                            _E(l) "\nFile: " _E(.)_E(>)_E(C) "%5")
                                    .arg(_item->info->gets("title"))
                                    .arg(packageId())
                                    .arg(_item->info->gets("version"))
                                    .arg(_item->info->gets("license"))
                                    .arg(_item->file->description()));
            return pop;
        }

    private:
        PackageItem const *_item;
        LabelWidget *_title;
        LabelWidget *_subtitle;
        QList<ButtonWidget *> _tags;
        ButtonWidget *_loadButton;
        PopupButtonWidget *_infoButton;
    };

    Instance(Public *i) : Base(i)
    {
        self.add(search = new LineEditWidget);
        search->rule()
                .setInput(Rule::Left,  self.rule().left())
                .setInput(Rule::Right, self.rule().right())
                .setInput(Rule::Top,   self.rule().top());
        search->setEmptyContentHint(tr("Search packages"));

        self.add(menu = new MenuWidget);
        menu->enableScrolling(false);
        menu->enablePageKeys(false);
        menu->setGridSize(1, ui::Expand, 0, ui::Expand);
        menu->rule()
                .setInput(Rule::Left,  self.rule().left())
                .setInput(Rule::Right, self.rule().right())
                .setInput(Rule::Top,   search->rule().bottom());
        menu->organizer().setWidgetFactory(*this);
    }

    void populate()
    {
        StringList packages = App::packageLoader().findAllPackages();
        qSort(packages);

        // Remove from the list those packages that are no longer listed.
        for(ui::DataPos i = 0; i < menu->items().size(); ++i)
        {
            if(!packages.contains(menu->items().at(i).data().toString()))
            {
                menu->items().remove(i--);
            }
        }

        // Add/update the listed packages.
        for(String const &path : packages)
        {
            File const &pack = App::rootFolder().locate<File>(path);

            // Core packages are mandatory and thus omitted.
            if(Package::tags(pack).contains("core")) continue;

            // Is this already in the list?
            ui::DataPos pos = menu->items().findData(pack.objectNamespace().gets("package.ID"));
            if(pos != ui::Data::InvalidPos)
            {
                menu->items().at(pos).as<PackageItem>().setFile(pack);
            }
            else
            {
                menu->items() << new PackageItem(pack);
            }
        }
    }

    GuiWidget *makeItemWidget(ui::Item const &item, GuiWidget const *)
    {
        return new ItemWidget(item.as<PackageItem>());
    }

    void updateItemWidget(GuiWidget &widget, ui::Item const &)
    {
        widget.as<ItemWidget>().updateContents();
    }
};

PackagesWidget::PackagesWidget(String const &name)
    : GuiWidget(name)
    , d(new Instance(this))
{
    /*buttons()
            << new DialogButtonItem(Default | Accept, tr("Close"))
            << new DialogButtonItem(Action, style().images().image("refresh"),
                                    new CallbackAction([this] ()
            {
                App::fileSystem().refresh();
                d->populate();
            }));*/

    //area().setContentSize(d->menu->rule().width(), d->menu->rule().height());
    rule().setInput(Rule::Height,
                    d->search->rule().height() +
                    d->menu->rule().height());

    refreshPackages();
}

void PackagesWidget::refreshPackages()
{
    App::fileSystem().refresh();
    d->populate();
}
