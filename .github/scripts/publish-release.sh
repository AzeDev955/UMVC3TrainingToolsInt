#!/usr/bin/env bash
set -euo pipefail

tag=${1:?Falta la versión compilada.}
if [[ ! $tag =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ || $(git rev-parse HEAD) != "$GITHUB_SHA" ]]; then
  echo 'La versión o el commit de publicación no coinciden con la compilación.' >&2
  exit 1
fi

releases=$(gh api --paginate "repos/$GITHUB_REPOSITORY/releases?per_page=100" \
  --jq '.[] | {tag: .tag_name, draft, prerelease, author: .author.login}')
stable=$(jq -rs '.[] | select(.draft == false and .prerelease == false) | .tag' <<< "$releases")
if grep -Fxq "$tag" <<< "$stable"; then
  echo "$tag ya está publicada."
  exit 0
fi
latest=$(printf '%s\n' "$stable" "$tag" | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' | sort -V | tail -n 1)
if [[ $latest != "$tag" ]]; then
  echo "$latest ya está publicada y es posterior a $tag."
  exit 0
fi

existing=$(jq -cs --arg tag "$tag" '[.[] | select(.tag == $tag)] | first // empty' <<< "$releases")
if [[ -n $existing ]] && ! jq -e '.draft and (.author == "github-actions[bot]")' <<< "$existing" >/dev/null; then
  echo "Ya existe una release de $tag ajena a este flujo de publicación." >&2
  exit 1
fi

notes="changelog/$tag.md"
asset="dist/UMVC3TrainingToolsInt-Setup.exe"
if [[ ! -s $notes ]]; then
  echo "Faltan las notas $notes." >&2
  exit 1
fi
mv "dist/UMVC3TrainingToolsInt-Setup-$tag.exe" "$asset"

ref="refs/tags/$tag"
if git show-ref --verify --quiet "$ref"; then
  if [[ $(git rev-parse "$ref^{commit}") != "$GITHUB_SHA" ]]; then
    echo "La etiqueta $tag apunta a otro commit." >&2
    exit 1
  fi
else
  gh api --silent --method POST "repos/$GITHUB_REPOSITORY/git/refs" -f "ref=$ref" -f "sha=$GITHUB_SHA"
fi

if [[ -n $existing ]]; then
  gh release upload "$tag" "$asset" --clobber
  gh release edit "$tag" --draft=false --latest --title "$tag" --notes-file "$notes"
else
  gh release create "$tag" "$asset" --verify-tag --latest --title "$tag" --notes-file "$notes"
fi
