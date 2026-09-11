$ErrorActionPreference = 'Stop'

$requiredPaths = @(
    'CHANGELOG.md',
    'CONTRIBUTING.md',
    'ROADMAP.md',
    'SECURITY.md',
    'docs/firmware-status-and-roadmap.md',
    'esphome/mcc-pro-native.yaml',
    'esphome/secrets.example.yaml'
)

$missingPaths = $requiredPaths | Where-Object { -not (Test-Path -LiteralPath $_) }
if ($missingPaths) {
    throw "Required public files are missing: $($missingPaths -join ', ')"
}

$trackedPrivatePaths = @(
    git ls-files --cached -- 'archive/**' 'firmware_dumps/**' 'esphome/secrets.yaml'
) | Where-Object { $_ }
if ($trackedPrivatePaths) {
    throw "Private local inputs are tracked: $($trackedPrivatePaths -join ', ')"
}

$ignoredPaths = @(
    'archive/private-note.txt',
    'artifacts/diagnostic.log',
    'firmware_dumps/extracted.bin',
    'esphome/secrets.yaml'
)
foreach ($ignoredPath in $ignoredPaths) {
    git check-ignore --no-index -q -- $ignoredPath
    if ($LASTEXITCODE -ne 0) {
        throw "Expected ignored path is not ignored: $ignoredPath"
    }
}

$archiveReferences = @(git grep -n -i -E 'archive/|archived local|archived locally|legacy config' -- '*.md' ':!docs/component-control-reference.md') | Where-Object { $_ }
if ($archiveReferences) {
    throw "Public documentation names local archive material: $($archiveReferences -join '; ')"
}

git diff --check
if ($LASTEXITCODE -ne 0) {
    throw 'Patch formatting check failed.'
}

Write-Host 'Repository quality checks passed.'