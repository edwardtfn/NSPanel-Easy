# Release Instructions

This document provides step-by-step instructions for releasing a new version of the project.
Follow these guidelines carefully to ensure a smooth and consistent release process.

## Prerequisites

1. Ensure you have the latest version of the `main` branch checked out.
2. Ensure all tests are passing locally.
3. Ensure you have the appropriate permissions to push to the main branch and create releases.

## Step-by-Step Release Process

### 1. Update Version Number

1. **Update the version number**:
   - Modify the version number in the following files:
     - **ESPHome**:
       - `nspanel_esphome_version.yaml`: Update the `substitutions` section with the version number of the Blueprint and TFT.
         Note that you shouldn't edit the ESPHome version, as that will be automatically updated during a release to GitHub.
     - **Blueprint**:
       - `nspanel_easy_blueprint.yaml`: Update the version number in both the blueprint description and the `variables` area.
     - **TFT**:
       - For all `.hmi` files (e.g., `nspanel_xxx.hmi`), update the `version` variable on the boot page. You can use `scripts/update_nextion_version.sh <new_version>` for that.

2. **Generate new TFT files**:
   - Compile each modified `.hmi` file to generate the updated `.tft` files. Use Nextion Editor v1.68.2 or later, as the project uses macros.
   - Run `scripts/generate_nextion_txt.sh` to generate the text files showing what's updated from the HMI.

3. **Create a new branch with the version name**:
   - After updating the version in all required files, create a new branch from the `dev` branch with the new version number prefixed with `v`:
     ```bash
     git checkout dev
     git pull origin dev
     git checkout -b vX.X.X
     git push origin vX.X.X
     ```
   - This branch ensures the `external_components` are validated correctly during testing.

4. **Merge changes back to `dev`**:
   - After making the changes, merge them back into the `dev` branch to trigger the CI/CD actions and validate the build process:
     ```bash
     git checkout dev
     git merge vX.X.X
     git push origin dev
     ```

### 2. Pre-Build Firmware Validation

1. **Verify pre-build firmware**:
   - Check the CI/CD actions on the `dev` branch to ensure the `pre-build` firmware file is successfully created.
   - If the `pre-build` firmware is missing or failed, investigate and resolve the issues before proceeding.

### 3. Merge Workflow (`dev` ->  `main`)

- Once `dev` is validated, merge `dev` into `main`.
- Ensure all changes are properly reflected and ready for release.

### 4. Post-Release Tasks

1. **Update the documentation**:
   - Ensure that all relevant documentation is updated to reflect the new version.
   - If necessary, update any online documentation or API references.

2. **Announce the release**:
   - Post the release announcement on your project's communication channels (e.g., Slack, Discord, internal mailing lists).
   - Consider posting on social media or community forums if applicable.

3. **Monitor for issues**:
   - Keep an eye on the issue tracker for any bugs or issues reported by users after the release.
