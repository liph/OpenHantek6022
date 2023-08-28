// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCloseEvent>
#include <QDebug>
#include <QDockWidget>
#include <QSignalBlocker>

#include <cmath>

#include "VoltageDock.h"
#include "dockwindows.h"

#include "dsosettings.h"
#include "sispinbox.h"
#include "utils/printutils.h"


template < typename... Args > struct SELECT {
    template < typename C, typename R > static constexpr auto OVERLOAD_OF( R ( C::*pmf )( Args... ) ) -> decltype( pmf ) {
        return pmf;
    }
};


VoltageDock::VoltageDock( DsoSettingsScope *scope, QWidget *parent )
    : QDockWidget( tr( "Voltage" ), parent ), scope( scope ) {

    if ( scope->verboseLevel > 1 )
        qDebug() << " VoltageDock::VoltageDock()";

    for ( auto e : Dso::MathModeEnum ) {
        modeStrings.append( Dso::mathModeString( e ) );
    }

    for ( double gainStep : scope->gainSteps ) {
        gainStrings << valueToString( gainStep, UNIT_VOLTS, 0 );
    }

    dockLayout = new QGridLayout();
    dockLayout->setColumnMinimumWidth( 0, 50 );
    dockLayout->setColumnStretch( 1, 1 ); // stretch ComboBox in 2nd (middle) column
    dockLayout->setColumnStretch( 2, 1 ); // stretch ComboBox in 3rd (last) column
    dockLayout->setSpacing( DOCK_LAYOUT_SPACING );
    // Initialize elements
    int row = 0;
    for ( ChannelID channel = 0; channel < scope->voltage.size(); ++channel ) {
        ChannelBlock b;

        if ( channel < scope->maxChannels )
            b.usedCheckBox = new QCheckBox( tr( "CH&%1" ).arg( channel + 1 ) ); // define shortcut <ALT>1 / <ALT>2
        else
            b.usedCheckBox = new QCheckBox( tr( "MA&TH" ) );
        b.miscComboBox = new QComboBox();
        b.gainComboBox = new QComboBox();
        if ( scope->toolTipVisible )
            b.gainComboBox->setToolTip( tr( "Voltage range per vertical screen division" ) );
        b.invertCheckBox = new QCheckBox( tr( "Invert" ) );
        b.attnSpinBox = new QSpinBox();
        if ( scope->toolTipVisible )
            b.attnSpinBox->setToolTip( tr( "Set probe attenuation, scroll or type a value to select" ) );
        b.attnSpinBox->setMinimum( ATTENUATION_MIN );
        b.attnSpinBox->setMaximum( ATTENUATION_MAX );
        b.attnSpinBox->setPrefix( tr( "x" ) );

        channelBlocks.push_back( std::move( b ) );

        if ( channel < scope->maxChannels ) {
            b.miscComboBox->addItems( couplingStrings );
            if ( scope->toolTipVisible )
                b.miscComboBox->setToolTip( tr( "Select DC or AC coupling" ) );
        } else {
            b.miscComboBox->addItems( modeStrings );
            if ( scope->toolTipVisible )
                b.miscComboBox->setToolTip( tr( "Select the mathematical operation for this channel" ) );
        }
        b.gainComboBox->addItems( gainStrings );

        if ( channel < scope->maxChannels ) {
            dockLayout->setColumnStretch( 1, 1 ); // stretch ComboBox in 2nd (middle) column 1x
            dockLayout->setColumnStretch( 2, 2 ); // stretch ComboBox in 3rd (last) column 2x
            dockLayout->addWidget( b.usedCheckBox, row, 0 );
            dockLayout->addWidget( b.gainComboBox, row++, 1, 1, 2 ); // fill 1 row, 2 col
            dockLayout->addWidget( b.invertCheckBox, row, 0 );
            dockLayout->addWidget( b.attnSpinBox, row, 1, 1, 1 );    // fill 1 row, 2 col
            dockLayout->addWidget( b.miscComboBox, row++, 2, 1, 1 ); // fill 1 row, 2 col
            // draw divider line
            QFrame *divider = new QFrame();
            divider->setLineWidth( 1 );
            divider->setFrameShape( QFrame::HLine );
            QPalette palette = QPalette();
            palette.setColor( QPalette::WindowText, QColor( 128, 128, 128 ) );
            divider->setPalette( palette ); // reduce the contrast of the divider
            dockLayout->addWidget( divider, row++, 0, 1, 3 );
        } else { // MATH function, all in one row
            dockLayout->addWidget( b.usedCheckBox, row, 0 );
            dockLayout->addWidget( b.gainComboBox, row, 1 );
            dockLayout->addWidget( b.miscComboBox, row, 2 );
        }

        connect( b.gainComboBox, SELECT< int >::OVERLOAD_OF( &QComboBox::currentIndexChanged ),
                 [ this, channel ]( unsigned index ) {
                     this->scope->voltage[ channel ].gainStepIndex = index;
                     emit gainChanged( channel, this->scope->gain( channel ) );
                 } );
        connect( b.attnSpinBox, SELECT< int >::OVERLOAD_OF( &QSpinBox::valueChanged ), [ this, channel ]( unsigned attnValue ) {
            this->scope->voltage[ channel ].probeAttn = attnValue;
            setAttn( channel, attnValue );
            emit probeAttnChanged( channel, attnValue ); // make sure to set the probe first, since this will influence the gain
            emit gainChanged( channel, this->scope->gain( channel ) );
        } );
        connect( b.invertCheckBox, &QAbstractButton::toggled, [ this, channel ]( bool checked ) {
            this->scope->voltage[ channel ].inverted = checked;
            emit invertedChanged( channel, checked );
        } );
        connect( b.miscComboBox, SELECT< int >::OVERLOAD_OF( &QComboBox::currentIndexChanged ),
                 [ this, channel, scope ]( int index ) {
                     this->scope->voltage[ channel ].couplingOrMathIndex = index;
                     if(index >= 0)
                        this->scope->voltage[channel].selectedChannelName = this->scope->AvaliableChannelNames[index];
//                     if ( channel < scope->maxChannels ) { // CH1 & CH2
//                         // setCoupling(channel, (unsigned)index);
//                         emit couplingChanged( channel, scope->coupling( channel, nullptr ) );
//                     } else { // MATH function changed
//                         Dso::MathMode mathMode = Dso::getMathMode( this->scope->voltage[ channel ] );
//                         setAttn( channel, this->scope->voltage[ channel ].probeAttn );
//                         emit modeChanged( mathMode );
//                         emit usedChannelChanged( channel, Dso::mathChannelsUsed( mathMode ) );
//                     }
                 } );
        connect( b.usedCheckBox, &QCheckBox::toggled, [ this, channel ]( bool checked ) {
            this->scope->voltage[ channel ].used = checked;
            this->scope->voltage[ channel ].visible = checked;
            unsigned mask = 0;
            if ( checked ) {
                if ( channel < this->scope->maxChannels )
                    mask = channel + 1;
                else
                    mask = Dso::mathChannelsUsed( Dso::MathMode( this->scope->voltage[ 2 ].couplingOrMathIndex ) );
            }
            emit usedChannelChanged( channel, mask ); // channel bit mask 0b01, 0b10, 0b11
        } );
    }

    // Load settings into GUI
    loadSettings( scope);

    dockWidget = new QWidget();
    SetupDockWidget( this, dockWidget, dockLayout );
}


void VoltageDock::loadSettings( DsoSettingsScope *scope) {
    if ( scope->verboseLevel > 2 )
        qDebug() << "  VDock::loadSettings()";
    for ( ChannelID channel = 0; channel < scope->voltage.size(); ++channel ) {
        if ( channel < scope->maxChannels ) {
            if ( int( scope->voltage[ channel ].couplingOrMathIndex ) < couplingStrings.size() )
                setCoupling( channel, scope->voltage[ channel ].couplingOrMathIndex );
        } else {
            setMode( scope->voltage[ channel ].couplingOrMathIndex );
        }

        setGain( channel, scope->voltage[ channel ].gainStepIndex );
        setUsed( channel, scope->voltage[ channel ].used );
        scope->voltage[ channel ].visible = scope->voltage[ channel ].used;
        setAttn( channel, scope->voltage[ channel ].probeAttn );
        setInverted( channel, scope->voltage[ channel ].inverted );
    }
}


/// \brief Don't close the dock, just hide it
/// \param event The close event that should be handled.
void VoltageDock::closeEvent( QCloseEvent *event ) {
    hide();
    event->accept();
}

void VoltageDock::onNewChannelData(const DsoSettingsScope* scope)
{
    couplingStrings.clear();
    for(const auto& channel : scope->AvaliableChannelNames)
    {
        couplingStrings << channel;
    }
    for(auto& channelBlock:channelBlocks)
    {
        int index = channelBlock.miscComboBox->currentIndex();
        channelBlock.miscComboBox->clear();
        channelBlock.miscComboBox->addItems(couplingStrings);
        channelBlock.miscComboBox->setCurrentIndex(index);
    }
}

void VoltageDock::onNewChannelData2()
{
    couplingStrings.clear();
    couplingStrings << "lll";
    for(auto& channelBlock:channelBlocks)
    {
        int index = channelBlock.miscComboBox->currentIndex();
        channelBlock.miscComboBox->clear();
        channelBlock.miscComboBox->addItems(couplingStrings);
        channelBlock.miscComboBox->setCurrentIndex(index);
    }
}

void VoltageDock::setCoupling( ChannelID channel, unsigned couplingIndex ) {
    if ( channel >= scope->maxChannels )
        return;

    if ( scope->verboseLevel > 2 )
        qDebug() << "  VDock::setCoupling()" << channel << couplingStrings[ int( couplingIndex ) ];
    QSignalBlocker blocker( channelBlocks[ channel ].miscComboBox );
    channelBlocks[ channel ].miscComboBox->setCurrentIndex( int( couplingIndex ) );
}


void VoltageDock::setGain( ChannelID channel, unsigned gainStepIndex ) {
    if ( channel >= scope->voltage.size() )
        return;
    if ( gainStepIndex >= scope->gainSteps.size() )
        return;
    if ( scope->verboseLevel > 2 )
        qDebug() << "  VDock::setGain()" << channel << gainStrings[ int( gainStepIndex ) ];
    QSignalBlocker blocker( channelBlocks[ channel ].gainComboBox );
    channelBlocks[ channel ].gainComboBox->setCurrentIndex( int( gainStepIndex ) );
}


void VoltageDock::setAttn( ChannelID channel, double attnValue ) {
    if ( scope->verboseLevel > 2 )
        qDebug() << "  VDock::setAttn()" << channel << attnValue;
    if ( channel >= scope->voltage.size() )
        return;
    QSignalBlocker blocker( channelBlocks[ channel ].gainComboBox );
    int index = channelBlocks[ channel ].gainComboBox->currentIndex();
    gainStrings.clear();

    // change unit to V² for the multiplying math functions
    if ( channel >= scope->maxChannels ) // MATH channel
        for ( double gainStep : scope->gainSteps )
            gainStrings << valueToString(
                gainStep * attnValue, Dso::mathModeUnit( Dso::MathMode( scope->voltage[ scope->maxChannels ].couplingOrMathIndex ) ),
                -1 ); // auto format V²
    else
        for ( double gainStep : scope->gainSteps )
            gainStrings << valueToString( gainStep * attnValue, UNIT_VOLTS, -1 ); // auto format V²

    channelBlocks[ channel ].gainComboBox->clear();
    channelBlocks[ channel ].gainComboBox->addItems( gainStrings );
    channelBlocks[ channel ].gainComboBox->setCurrentIndex( index );
    scope->voltage[ channel ].probeAttn = attnValue;
    channelBlocks[ channel ].attnSpinBox->setValue( int( attnValue ) );
}


void VoltageDock::setMode( unsigned mathModeIndex ) {
    if ( scope->verboseLevel > 2 )
        qDebug() << "  VDock::setMode()" << modeStrings[ int( mathModeIndex ) ];
    QSignalBlocker blocker( channelBlocks[ scope->maxChannels ].miscComboBox );
    channelBlocks[ scope->maxChannels ].miscComboBox->setCurrentIndex( int( mathModeIndex ) );
}


void VoltageDock::setUsed( ChannelID channel, bool used ) {
    if ( channel >= scope->voltage.size() )
        return;
    if ( scope->verboseLevel > 2 )
        qDebug() << "  VDock::setUsed()" << channel << used;
    QSignalBlocker blocker( channelBlocks[ channel ].usedCheckBox );
    channelBlocks[ channel ].usedCheckBox->setChecked( used );
}


void VoltageDock::setInverted( ChannelID channel, bool inverted ) {
    if ( channel >= scope->voltage.size() )
        return;
    if ( scope->verboseLevel > 2 )
        qDebug() << "  VDock::setInverted()" << channel << inverted;
    QSignalBlocker blocker( channelBlocks[ channel ].invertCheckBox );
    channelBlocks[ channel ].invertCheckBox->setChecked( inverted );
}
