#include "stdafx.h"

#include "MainForm.h"
#include <QtCore/QSettings>
#include <QtWidgets/QApplication>

#include "model.h"
#include "textures.h"

int main ( int argc, char *argv[] )
{
    QApplication app (argc, argv);
    QSettings settings ("OpenJK", "ModView");
    
#if defined(__APPLE__) && defined(__MACH__)
    app.setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
#endif

    App_OnceOnly();
    FakeCvars_OnceOnlyInit();

    MainForm w (settings);
    w.show();

    int errorCode = app.exec();

    Media_Delete();
    FakeCvars_Shutdown();

    return errorCode;
}