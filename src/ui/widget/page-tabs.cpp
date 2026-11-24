// SPDX-License-Identifier: GPL-2.0-or-later
#include "page-tabs.h"

#include <algorithm>
#include <glibmm/i18n.h>
#include <glibmm/main.h>
#include "desktop.h"
#include "document-undo.h"
#include "object/sp-page.h"
#include "page-manager.h"

namespace Inkscape::UI::Widget {

PageTabs::PageTabs()
    : _viewport(_scroll.get_hadjustment(), _scroll.get_vadjustment())
{
    set_name("PageTabs");
    auto control = [this](Gtk::Button &button, char const *symbol, char const *tip, auto action) {
        button.set_label(symbol);
        button.set_tooltip_text(tip);
        button.set_focus_on_click(false);
        button.signal_clicked().connect(action);
        append(button);
    };
    control(_before, "⊕", _("Insert page before"), [this] { insert(true); });
    control(_first, "⏮", _("First page"), [this] { select(0); });
    control(_prev, "◀", _("Previous page"), [this] { select(_document->getPageManager().getSelectedPageIndex() - 1); });
    append(_count);
    control(_next, "▶", _("Next page"), [this] { select(_document->getPageManager().getSelectedPageIndex() + 1); });
    control(_last, "⏭", _("Last page"), [this] { select(_document->getPageManager().getPageCount() - 1); });
    control(_after, "⊕", _("Insert page after"), [this] { insert(false); });
    _scroll.set_policy(Gtk::PolicyType::EXTERNAL, Gtk::PolicyType::NEVER);
    _scroll.set_hexpand(true);
    _viewport.set_child(_tabs);
    _scroll.set_child(_viewport);
    append(_scroll);
    set_sensitive(false);
}

void PageTabs::setDesktop(SPDesktop *desktop)
{
    _replacement.disconnect();
    _desktop = desktop;
    bind(desktop ? desktop->getDocument() : nullptr);
    if (desktop) {
        _replacement = desktop->connectDocumentReplaced(sigc::hide<0>(sigc::mem_fun(*this, &PageTabs::bind)));
    }
}

void PageTabs::bind(SPDocument *document)
{
    _pages.disconnect();
    _selection.disconnect();
    _document = document;
    if (document) {
        auto &manager = document->getPageManager();
        _pages = manager.connectPagesChanged([this](SPPage *) { rebuild(); });
        _selection = manager.connectPageSelected([this](SPPage *) { update(); });
    }
    set_sensitive(document != nullptr);
    rebuild();
}

void PageTabs::rebuild()
{
    _idle.disconnect();
    _labels.clear();
    for (auto button : _buttons) {
        _tabs.remove(*button);
    }
    _buttons.clear();
    if (!_document) {
        _count.set_text("");
        return;
    }
    auto &manager = _document->getPageManager();
    for (int i = 0; i < std::max(1, manager.getPageCount()); ++i) {
        auto button = Gtk::make_managed<Gtk::Button>();
        auto page = manager.getPage(i);
        auto label = Gtk::make_managed<Gtk::Label>();
        label->set_ellipsize(Pango::EllipsizeMode::END);
        label->set_max_width_chars(18);
        auto refresh = [button, label, page] {
            Glib::ustring name = page ? Glib::ustring(page->getLabel()) : Glib::ustring::compose(_("Page %1"), 1);
            label->set_text(name);
            button->set_tooltip_text(name);
        };
        refresh();
        if (page) {
            _labels.emplace_back(page->connectModified([refresh](SPObject *, unsigned) { refresh(); }));
        }
        button->set_child(*label);
        button->add_css_class("page-tab");
        button->set_focus_on_click(false);
        button->signal_clicked().connect([this, i] { select(i); });
        _tabs.append(*button);
        _buttons.push_back(button);
    }
    update();
}

void PageTabs::update()
{
    if (!_document || _buttons.empty()) {
        return;
    }
    int last = static_cast<int>(_buttons.size()) - 1;
    int current = std::clamp(_document->getPageManager().getSelectedPageIndex(), 0, last);
    _count.set_text(Glib::ustring::compose(_("%1 de %2"), current + 1, last + 1));
    _first.set_sensitive(current > 0);
    _prev.set_sensitive(current > 0);
    _next.set_sensitive(current < last);
    _last.set_sensitive(current < last);
    for (int i = 0; i <= last; ++i) {
        if (i == current) {
            _buttons[i]->add_css_class("current");
        } else {
            _buttons[i]->remove_css_class("current");
        }
    }
    _idle = Glib::signal_idle().connect([this, current] {
        _viewport.scroll_to(*_buttons[current]);
        return false;
    });
}

void PageTabs::select(int index)
{
    auto &manager = _document->getPageManager();
    if (auto page = manager.getPage(index)) {
        manager.selectPage(page);
        manager.centerToPage(_desktop, page);
    }
}

void PageTabs::insert(bool before)
{
    auto &manager = _document->getPageManager();
    int pos = std::max(0, manager.getSelectedPageIndex()) + (before ? 0 : 1);
    if (auto page = manager.newPage()) {
        page->setPageIndex(pos, false);
        manager.selectPage(page);
        manager.centerToPage(_desktop, page);
        DocumentUndo::done(_document, RC_("Undo", "New Automatic Page"), "tool-pages");
    }
}
}
