param(
    [string]$Tag,
    [string]$From,
    [string]$To,
    [switch]$DryRun
)

# The script has Claude locally summarize the commits since the last published release and splices the result into a draft release's
# notes, after its existing text
# 
# -DryRun prints the notes instead of editing the release.

$ErrorActionPreference = "Stop"

$utf8 = New-Object System.Text.UTF8Encoding $false
$OutputEncoding = $utf8
[Console]::OutputEncoding = $utf8

$claude = "$env:USERPROFILE\.local\bin\claude.exe"
$command = Get-Command claude -ErrorAction SilentlyContinue

if ($command)
{
    $claude = $command.Source
}

$heading = "## What's changed"

git fetch origin --tags --quiet

$body = ""

if (-not $To)
{
    if (-not $Tag)
    {
        $Tag = gh release list --limit 20 --json tagName,isDraft -q '[.[] | select(.isDraft)][0].tagName'

        if (-not $Tag)
        {
            throw "No draft release found"
        }
    }

    $release = gh release view $Tag --json body,targetCommitish | ConvertFrom-Json
    $To = $release.targetCommitish
    $body = [string]$release.body
}

if (-not $From)
{
    $From = gh release list --exclude-drafts --limit 1 --json tagName -q '.[0].tagName'
}

$log = git log --no-merges --format="- %h %s%n%b" "$From..$To"

if (-not $log)
{
    throw "No commits between $From and $To"
}

$prompt = @"
Write release notes for Hyperion Engine (a game engine and editor) covering the commits given on stdin, which run from
$From to $To. Group changes under a few short ### headings by area (for example Rendering, Editor, Scripting, Web,
Build). Write one bullet per user-visible change in plain language, merging commits that are parts of the same change.
Skip version bumps, changes that were later reverted, and pure housekeeping. When a commit message is too vague to
understand, look at the commit with git show --stat or git show. Output only the markdown: no preamble, no top-level
heading.
"@

$notes = ($log -join "`n") | & $claude -p $prompt --allowedTools "Bash(git show:*)" "Bash(git log:*)" "Bash(git diff:*)"

if ($LASTEXITCODE -ne 0)
{
    throw "claude exited with ${LASTEXITCODE}: $notes"
}

$notes = ($notes -join "`n").Trim()

if ($DryRun)
{
    Write-Output $notes
    return
}

$repo = gh repo view --json nameWithOwner -q .nameWithOwner
$marker = $body.IndexOf($heading)

if ($marker -ge 0)
{
    $body = $body.Substring(0, $marker)
}

$body = $body.TrimEnd() + "`n`n$heading`n`n$notes`n`n**Full changelog**: https://github.com/$repo/compare/$From...$Tag`n"

$file = New-TemporaryFile
[System.IO.File]::WriteAllText($file.FullName, $body, $utf8)
gh release edit $Tag --notes-file $file
Remove-Item $file

Write-Host "Updated release notes for $Tag ($From..$To)"
