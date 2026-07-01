#pragma once
#include <vector>
#include <string>

void StartProcessing(const std::vector<std::string>& files);
void
AppMenuBar();
void
RenderUI();
bool
InitializeNativeFileDialogs();
void
ShutdownNativeFileDialogs();
bool
NativeFileDialogsAvailable();

void SetDragging(bool dragging);
bool IsDragging();
