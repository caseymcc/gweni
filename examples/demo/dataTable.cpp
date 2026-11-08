#include "demo/dataTable.h"
#include "gweni/controls/dataTable.h"
#include "gweni/controls/label.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

namespace gweni
{

namespace
{

struct Element
{
    const char *name;
    const char *symbol;
    int number;
    const char *mass;
};

// The first 40 elements: names and symbols sort as text, the atomic number
// and mass as numbers.
const Element elements[DataTableDemo::RowCount]=
{
    {"Hydrogen", "H", 1, "1.008"},       {"Helium", "He", 2, "4.0026"},
    {"Lithium", "Li", 3, "6.94"},        {"Beryllium", "Be", 4, "9.0122"},
    {"Boron", "B", 5, "10.81"},          {"Carbon", "C", 6, "12.011"},
    {"Nitrogen", "N", 7, "14.007"},      {"Oxygen", "O", 8, "15.999"},
    {"Fluorine", "F", 9, "18.998"},      {"Neon", "Ne", 10, "20.180"},
    {"Sodium", "Na", 11, "22.990"},      {"Magnesium", "Mg", 12, "24.305"},
    {"Aluminium", "Al", 13, "26.982"},   {"Silicon", "Si", 14, "28.085"},
    {"Phosphorus", "P", 15, "30.974"},   {"Sulfur", "S", 16, "32.06"},
    {"Chlorine", "Cl", 17, "35.45"},     {"Argon", "Ar", 18, "39.95"},
    {"Potassium", "K", 19, "39.098"},    {"Calcium", "Ca", 20, "40.078"},
    {"Scandium", "Sc", 21, "44.956"},    {"Titanium", "Ti", 22, "47.867"},
    {"Vanadium", "V", 23, "50.942"},     {"Chromium", "Cr", 24, "51.996"},
    {"Manganese", "Mn", 25, "54.938"},   {"Iron", "Fe", 26, "55.845"},
    {"Cobalt", "Co", 27, "58.933"},      {"Nickel", "Ni", 28, "58.693"},
    {"Copper", "Cu", 29, "63.546"},      {"Zinc", "Zn", 30, "65.38"},
    {"Gallium", "Ga", 31, "69.723"},     {"Germanium", "Ge", 32, "72.630"},
    {"Arsenic", "As", 33, "74.922"},     {"Selenium", "Se", 34, "78.971"},
    {"Bromine", "Br", 35, "79.904"},     {"Krypton", "Kr", 36, "83.798"},
    {"Rubidium", "Rb", 37, "85.468"},    {"Strontium", "Sr", 38, "87.62"},
    {"Yttrium", "Y", 39, "88.906"},      {"Zirconium", "Zr", 40, "91.224"},
};

const char *const captions[]={"Element", "Symbol", "Number", "Mass"};

}//namespace

// The first 40 elements in a striped table with row and column rules, sorted
// by name. A header click sorts the rows by that column (numbers as numbers),
// and the selection stays on its element.
GWENI_CONTROL_CONSTRUCTOR(DataTableDemo)
{
    setDock(DockPosition::Center);

    controls::Label *title=newChild<controls::Label>();

    title->setText("The first 40 elements: click a header to sort, drag its edge to resize");
    title->setPos(10, 10);

    m_table=newChild<controls::DataTable>();
    m_table->setBounds(10, 34, 420, 400);
    m_table->setColumnCount(4);
    for(int i=0; i < 4; ++i)
        m_table->setColumnCaption(i, captions[i]);
    m_table->setColumnWidth(0, 130);
    m_table->setColumnWidth(1, 70);
    m_table->setColumnWidth(2, 80);
    m_table->setBorders(controls::DataTableBorders::All);

    for(const Element &element : elements)
    {
        controls::DataTableRow *row=m_table->addRow();

        row->setCellText(0, element.name);
        row->setCellText(1, element.symbol);
        row->setCellText(2, std::to_string(element.number));
        row->setCellText(3, element.mass);
        if(element.number == 6)
            m_table->setSelectedRow(row);
    }

    m_sorted=newChild<controls::Label>();
    m_sorted->setPos(444, 34);
    m_selected=newChild<controls::Label>();
    m_selected->setPos(444, 58);

    m_table->onSortChanged.add(this, &DataTableDemo::onSortChanged);
    m_table->onSelectionChange.add(this, &DataTableDemo::onSelectionChange);
    m_table->setSortColumn(0, controls::SortDirection::Ascending);
    onSelectionChange(event::Info(m_table));
}

// The rows in the clicked column's order: the number and mass columns by
// value, the others by text; descending is the reverse.
void DataTableDemo::onSortChanged(event::Info)
{
    const int column=m_table->getSortColumn();

    if(column < 0)
    {
        m_sorted->setText("Not sorted");
        return;
    }

    std::vector<controls::DataTableRow *> rows=m_table->getRows();
    const bool numeric=(column >= 2);
    const bool descending=(m_table->getSortDirection() == controls::SortDirection::Descending);

    std::stable_sort(rows.begin(), rows.end(),
        [column, numeric, descending](controls::DataTableRow *a, controls::DataTableRow *b)
        {
            const String &left=a->getCellText(column);
            const String &right=b->getCellText(column);
            const bool less=numeric?(std::atof(left.c_str()) < std::atof(right.c_str())):(left < right);
            const bool greater=numeric?(std::atof(right.c_str()) < std::atof(left.c_str())):(right < left);

            return descending?greater:less;
        });
    m_table->setRowOrder(rows);

    m_sorted->setText(String("Sorted by ")+captions[column]+
        (descending?", descending":", ascending"));
}

void DataTableDemo::onSelectionChange(event::Info)
{
    controls::DataTableRow *row=m_table->getSelectedRow();

    m_selected->setText(row?"Selected: "+row->getCellText(0):String("Selected: none"));
}

}//namespace gweni
