/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

// include the required headers
#include "UnitScaleWindow.h"
#include <QLabel>
#include <QSizePolicy>
#include <QPixmap>
#include <QCheckBox>
#include <QSettings>
#include <QVBoxLayout>
#include <QPushButton>


namespace EMStudio
{
    // constructor
    UnitScaleWindow::UnitScaleWindow(QWidget* parent)
        : QDialog(parent)
    {
        mScaleFactor = 1.0f;
        setModal(true);

        setWindowTitle("Scale Factor Setup");
        setObjectName("StyledWidgetDark");
        setFixedSize(220, 107);

        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setMargin(0);

        QVBoxLayout* topLayout = new QVBoxLayout();

        QLabel* topLabel = new QLabel("<b>Please setup a scale factor:</b>");
        topLabel->setStyleSheet("background-color: rgb(40, 40, 40); padding: 6px;");
        topLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        topLayout->addWidget(topLabel);

        layout->addLayout(topLayout);

        QHBoxLayout* scaleLayout = new QHBoxLayout();
        scaleLayout->setMargin(9);

        scaleLayout->addWidget(new QLabel("Scale Factor:"));

        mScaleSpinBox = new MysticQt::DoubleSpinBox();
        mScaleSpinBox->setRange(0.00001, 100000.0f);
        mScaleSpinBox->setSingleStep(0.01);
        mScaleSpinBox->setDecimals(7);
        mScaleSpinBox->setValue(1.0f);
        scaleLayout->addWidget(mScaleSpinBox);

        layout->addLayout(scaleLayout);

        QHBoxLayout* hLayout = new QHBoxLayout();
        hLayout->setContentsMargins(9, 0, 9, 9);

        mOK     = new QPushButton("OK");
        mCancel = new QPushButton("Cancel");
        hLayout->addWidget(mOK);
        hLayout->addWidget(mCancel);

        layout->addLayout(hLayout);

        connect(mOK, &QPushButton::clicked, this, &UnitScaleWindow::OnOKButton);
        connect(mCancel, &QPushButton::clicked, this, &UnitScaleWindow::OnCancelButton);
    }


    // destructor
    UnitScaleWindow::~UnitScaleWindow()
    {
    }


    // accept
    void UnitScaleWindow::OnOKButton()
    {
        mScaleFactor = static_cast<float>(mScaleSpinBox->value());
        emit accept();
    }


    // reject
    void UnitScaleWindow::OnCancelButton()
    {
        emit reject();
    }
} // namespace EMStudio

#include <EMotionFX/Tools/EMotionStudio/EMStudioSDK/Source/UnitScaleWindow.moc>