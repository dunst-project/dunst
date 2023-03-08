# Main repo
 - [ ] Update the changelog
 - [ ] Write release notes (Only if non-patch release)
 - [ ] Verify that the working directory is clean and on the master branch
 - [ ] Change the version in the Makefile to "x.x.x (iso-date)" (ex. "1.9.0 (2022-06-27)")
 - [ ] Commit changes (Commit title: `Dunst vX.X.X`)
 - [ ] Tag commit, make sure it's an annotated tag (`git tag -a "vX.X.X" -m "Dunst vX.X.X"`)
 - [ ] Push commits and tags

# Dunst-project.org
 - [ ] Run the update script (`REPO=../dunst ./update_new_release.sh OLDVER NEWVER`, ex. `REPO=../dunst ./update_new_release.sh 1.9.0 1.9.1`
 - [ ] Verify that they look fine when rendered (`hugo serve`)
 - [ ] Commit changes
 - [ ] Run deploy script and push (`./deploy.sh -p`)

# Main repo
 - [ ] Copy release notes to githubs release feature
 - [ ] Publish release on github

 - [ ] Update Makefile version to "x.x.x-non-git"
 - [ ] Commit & push
