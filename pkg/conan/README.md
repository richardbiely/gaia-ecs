# Gaia-ECS Conan Packaging

This directory contains the Gaia-ECS Conan 2 recipe and its `test_package`.

## Local validation

Validate the recipe before a release:

```bash
conan create pkg/conan --build=missing -s build_type=Release
conan create pkg/conan --build=missing -s build_type=Debug
```

The repository CI runs the same flow in [`.github/workflows/conan.yml`](../../.github/workflows/conan.yml).

## Public publishing

There are two practical public distribution paths:

1. Upload Gaia-ECS to your own Conan remote, such as JFrog Artifactory.
2. Submit a recipe to ConanCenter.

### Artifactory publish flow

The repository contains a manual GitHub Actions workflow at [`.github/workflows/conan-publish.yml`](../../.github/workflows/conan-publish.yml).
It validates the package with `conan create` and then uploads `gaia-ecs/<version>` to the configured remote.

Required GitHub Actions secrets:

- `CONAN_REMOTE_URL`: Conan remote URL, for example `https://example.jfrog.io/artifactory/api/conan/gaia-conan`
- `CONAN_REMOTE_USERNAME`: Conan remote username
- `CONAN_REMOTE_PASSWORD`: Conan remote password or access token

Suggested release steps:

1. Cut the Gaia-ECS release tag, for example `v0.9.3`.
2. Update [`pkg/conan/conanfile.py`](./conanfile.py) to the release version.
3. Update [`pkg/conan/conandata.yml`](./conandata.yml) with the release tarball URL and final SHA-256.
4. Wait for [`.github/workflows/conan.yml`](../../.github/workflows/conan.yml) to pass.
5. Run [`.github/workflows/conan-publish.yml`](../../.github/workflows/conan-publish.yml) with the same version.

The publish workflow refuses to upload if the requested version does not match the recipe version. When run from a tag, it also verifies the tag matches `v<version>`.

### ConanCenter submission checklist

ConanCenter is maintained through the [`conan-io/conan-center-index`](https://github.com/conan-io/conan-center-index) repository. Gaia-ECS is not uploaded there directly from this repository.

Use this checklist when preparing a ConanCenter submission:

1. Cut and verify the Gaia-ECS release tag first.
2. Confirm the release tarball URL and SHA-256 in [`pkg/conan/conandata.yml`](./conandata.yml).
3. Run `conan create pkg/conan --build=missing -s build_type=Release` locally or in CI.
4. Copy the recipe into the ConanCenter recipe layout and adapt it to ConanCenter policy if needed.
5. Open a pull request against `conan-io/conan-center-index`.
6. Wait for ConanCenter review and CI before treating the package as publicly available there.

The in-repo recipe is the staging recipe for Gaia-ECS releases. ConanCenter may still require naming, metadata, or recipe-structure adjustments before accepting it.
