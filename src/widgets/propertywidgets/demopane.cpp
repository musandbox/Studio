#include "demopane.h"

#include "../checkboxproperty.h"
#include "../combobox.h"
#include "../texturepicker.h"
#include "../textinput.h"
#include "../labelwidget.h"

#include <QDebug>

DemoPane::DemoPane()
{
    demoSlider  = this->addFloatValueSlider("Demo Slider", 1.f, 1000.f);
    demoCheck   = this->addCheckBox("Demo Checkbox", true);

    demoCombo   = this->addComboBox("Demo Combo");
    demoCombo->addItem("Circle");
    demoCombo->addItem("Square");

    demoPicker  = this->addTexturePicker("Demo Picker");

    demoInput   = this->addTextInput("Demo Text Input");
    demoColor   = this->addColorPicker("Demo Color");

    demoLabel   = this->addLabel("Demo Label");
    demoLabel->setText("The fat fox runs!");

    qDebug() << "demo prop " << this->height();
}
