# SPDX-License-Identifier: MPL-2.0
#
# This file is part of COVESA DLT-Viewer project.

add_library(dltdiagramplugin MODULE
    dltdiagramplugin.h
    dltdiagramplugin.cpp
    diagrammodel.h
    diagrammodel.cpp
    diagramengine.h
    diagramengine.cpp
    diagramgraphicsview.h
    diagramgraphicsview.cpp
    diagrambuilderdialog.h
    diagrambuilderdialog.cpp
    form.h
    form.cpp
)

target_link_libraries(dltdiagramplugin qdlt ${QT_PREFIX}::Widgets)

add_plugin(dltdiagramplugin)
