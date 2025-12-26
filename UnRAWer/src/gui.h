#pragma once
#include <vector>
#include <string>

void StartProcessing(const std::vector<std::string>& files);
void
AppMenuBar();
void
RenderUI();

void SetDragging(bool dragging);
bool IsDragging();
