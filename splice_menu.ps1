$lines = Get-Content 'src\ui\menu.cpp' -Encoding UTF8
$newPart = Get-Content 'src\ui\menu.cpp.skinpart.tmp' -Encoding UTF8
$before = $lines[0..1314]
$after = $lines[1759..($lines.Length-1)]
$combined = @() + $before + $newPart + $after
Set-Content 'src\ui\menu.cpp' -Value $combined -Encoding UTF8
Write-Host "Done, total lines: $($combined.Length)"
