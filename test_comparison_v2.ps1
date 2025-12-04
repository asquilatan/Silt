$silt = "..\bin\silt.exe"
$testDir = "test_comparison_v2"

if (Test-Path $testDir) { Remove-Item -Recurse -Force $testDir }
mkdir $testDir
cd $testDir

Write-Host "--- INITIALIZING GIT REPO ---"
git init
"hello" | Out-File hello.txt -Encoding ascii
git add hello.txt
git commit -m "Initial commit"
git tag v1.0

Write-Host "`n--- COMPARISON: SHOW-REF ---"
Write-Host "GIT:" -ForegroundColor Cyan
git show-ref
Write-Host "SILT:" -ForegroundColor Green
& $silt show-ref

Write-Host "`n--- COMPARISON: TAG LIST (Initial) ---"
Write-Host "GIT:" -ForegroundColor Cyan
git tag
Write-Host "SILT:" -ForegroundColor Green
& $silt tag

Write-Host "`n--- ACTION: SILT TAG v2.0 ---"
& $silt tag v2.0

Write-Host "`n--- COMPARISON: TAG LIST (After Silt Tag) ---"
Write-Host "GIT:" -ForegroundColor Cyan
git tag
Write-Host "SILT:" -ForegroundColor Green
& $silt tag

Write-Host "`n--- VERIFICATION: CHECKING v2.0 REF ---"
if (Test-Path .git/refs/tags/v2.0) {
    Get-Content .git/refs/tags/v2.0
} else {
    Write-Host "Error: v2.0 tag file not found!" -ForegroundColor Red
}

cd ..
