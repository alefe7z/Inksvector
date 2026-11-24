// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Calculator dialog backed by libqalculate (Extensions > MATH).
 */
/* Copyright (C) 2026 Inksvector contributors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_DIALOG_CALCULATOR_H
#define INKSCAPE_UI_DIALOG_CALCULATOR_H

#include <memory>
#include <string>
#include <vector>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/dropdown.h>
#include <gtkmm/entry.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/spinbutton.h>

#include "ui/dialog/dialog-base.h"

namespace Inkscape::UI::Dialog {

/**
 * Scientific calculator dialog.
 *
 * The math engine is libqalculate (https://github.com/Qalculate/libqalculate), the library
 * behind the Qalculate! desktop calculator. It supports arbitrary precision, units and
 * unit conversion ("5 ft to m"), currencies, physical constants, symbolic results,
 * functions, variables, number bases ("255 to hex") and more.
 */
class CalculatorDialog final : public DialogBase
{
public:
    CalculatorDialog();
    ~CalculatorDialog() override;

    void focus_dialog() override;

private:
    struct HistoryEntry
    {
        Glib::ustring expression;
        Glib::ustring result;
    };

    Gtk::Box _main;
    Gtk::ScrolledWindow _history_scroll;
    Gtk::ListBox _history_list;
    Gtk::Label _parsed_label;
    Gtk::Label _result_label;
    Gtk::Label _message_label;
    Gtk::Entry _entry;
    Gtk::Grid _keypad;
    Gtk::Box _options;
    Gtk::DropDown _angle_dropdown;
    Gtk::DropDown _base_dropdown;
    Gtk::CheckButton _exact_check;
    Gtk::CheckButton _decimal_comma_check;
    Gtk::SpinButton _precision_spin;
    Gtk::Button _copy_button;
    Gtk::Button _clear_history_button;
    Gtk::Button _rates_button;
    Gtk::Button *_decimal_button = nullptr; ///< Keypad key showing the current decimal separator.
    Gtk::Button *_comma_button = nullptr;   ///< Keypad key showing the current argument separator.

    std::vector<HistoryEntry> _history;
    int _history_cursor = -1; ///< Position while browsing history with Up/Down, -1 when not browsing.
    Glib::ustring _last_result;
    std::shared_ptr<bool> _alive; ///< Lets the exchange-rate worker know whether the dialog still exists.

    void build_keypad();
    void build_options();
    void insert_text(Glib::ustring const &text);
    void backspace();
    void clear_entry();
    void calculate();
    void add_history(Glib::ustring const &expression, Glib::ustring const &result);
    void recall_history(int direction);
    void copy_result();
    void update_exchange_rates();
    void apply_options();
    void show_message(Glib::ustring const &text, bool is_error);
};

} // namespace Inkscape::UI::Dialog

#endif // INKSCAPE_UI_DIALOG_CALCULATOR_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
