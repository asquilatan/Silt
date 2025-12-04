$silt = "..\bin\silt.exe"
$testDir = "test_recursive"

if (Test-Path $testDir) { Remove-Item -Recurse -Force $testDir }
mkdir $testDir
cd $testDir

Write-Host "--- INITIALIZING GIT REPO ---"
git init
"content" | Out-File file.txt -Encoding ascii
git add file.txt
git commit -m "Commit 1"
$commit1 = git rev-parse HEAD

"content2" | Out-File file2.txt -Encoding ascii
git add file2.txt
git commit -m "Commit 2"
$commit2 = git rev-parse HEAD

Write-Host "`n--- CREATING NESTED REFS (Simulating branches) ---"
# Git doesn't easily let us create arbitrary refs via CLI without update-ref
git update-ref refs/remotes/origin/main $commit1
git update-ref refs/tags/nested/v1.0 $commit1

Write-Host "`n--- COMPARISON: SHOW-REF (Recursive) ---"
Write-Host "GIT:" -ForegroundColor Cyan
git show-ref
Write-Host "SILT:" -ForegroundColor Green
& $silt show-ref

Write-Host "`n--- ACTION: SILT TAG (Lightweight on specific commit) ---"
& $silt tag v_light $commit1

Write-Host "`n--- ACTION: SILT TAG (Annotated) ---"
# Note: Silt's tag_create hardcodes the message currently, but we test the object creation
& $silt tag -a v_annotated

Write-Host "`n--- COMPARISON: TAG LIST ---"
Write-Host "GIT:" -ForegroundColor Cyan
git tag
Write-Host "SILT:" -ForegroundColor Green
& $silt tag

Write-Host "`n--- VERIFICATION: ANNOTATED TAG OBJECT ---"
# Check if v_annotated points to a tag object, not a commit
$tagSha = git rev-parse refs/tags/v_annotated
$objType = git cat-file -t $tagSha
Write-Host "Object Type for v_annotated: $objType"
if ($objType -eq "tag") {
    Write-Host "PASS: Annotated tag created tag object" -ForegroundColor Green
} else {
    Write-Host "FAIL: Annotated tag is not a tag object" -ForegroundColor Red
}

cd ..
