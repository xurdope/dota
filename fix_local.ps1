$content = Get-Content 'src\ui\menu.cpp' -Encoding UTF8
# Remove lines 1646-1653 (localPlayerAddr block, 1-indexed → 0-indexed: 1645..1652)
$before = $content[0..1644]
$after  = $content[1653..($content.Length-1)]
$combined = $before + "            // 2. Scan tick counter" + "            ImGui::TextColored(T.text_dim, `"Scan tick:`"); ImGui::SameLine(); ImGui::TextColored(col_cyan, `"%u`", stats.tickCount);" + "" + $after
Set-Content 'src\ui\menu.cpp' -Value $combined -Encoding UTF8
Write-Host "Done: $($combined.Length) lines"
