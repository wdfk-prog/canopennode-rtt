[中文](../zh/submodule-update.md)

# Submodule update guide

This repository expects upstream CANopenNode to be available as the `CANopenNode` git submodule in the repository root.

## 1. Clone with submodules

For a new checkout:

```sh
git clone --recursive <repo-url> canopennode-rtt
cd canopennode-rtt
```

Equivalent two-step form:

```sh
git clone <repo-url> canopennode-rtt
cd canopennode-rtt
git submodule update --init --recursive
```

## 2. Verify submodule presence

From the repository root:

```sh
test -d CANopenNode/301 && echo "CANopenNode submodule is present"
```

The build requires source files under directories such as:

```text
CANopenNode/301/
CANopenNode/303/
CANopenNode/304/
CANopenNode/305/
CANopenNode/309/
CANopenNode/storage/
CANopenNode/extra/
```

Only enabled Kconfig feature groups are compiled, but the submodule checkout should still be initialized consistently.

## 3. Update the repository and submodule

To update the wrapper and the submodule to the committed submodule revision:

```sh
git pull
git submodule update --init --recursive
```

If the parent repository updates the recorded CANopenNode submodule commit, this command moves `CANopenNode/` to that recorded commit.

## 4. Update CANopenNode intentionally

To move the submodule to a newer upstream revision, do it as an explicit source change:

```sh
cd CANopenNode
git fetch origin
git checkout <target-commit-or-branch>
cd ..
git status
```

Then build and test the affected Kconfig combinations before committing the new submodule pointer.

Recommended validation after updating CANopenNode:

1. Default package build.
2. SDO server enabled with segmented transfer.
3. PDO/SYNC enabled.
4. LSS slave enabled if the product uses commissioning.
5. Storage backend used by the product.
6. Debug logging configuration if `RT_USING_ULOG` is enabled.
7. Any product-specific custom OD build.

## 5. Pinning policy

For product firmware, prefer a pinned submodule commit over tracking a moving branch. A pinned commit gives deterministic builds and makes Kconfig/source compatibility easier to review.

Use a branch checkout only for active development and convert it to a reviewed commit pointer before release.

## 6. Common errors

### `CANopenNode submodule is missing`

Cause: `CANopenNode/301` is absent.

Fix:

```sh
git submodule update --init --recursive
```

Run the command from the repository root.

### Missing CANopenNode source file

Cause: the selected Kconfig feature requires a source file that is absent from the current submodule checkout.

Fix:

1. Confirm the submodule is initialized.
2. Confirm the submodule commit matches the wrapper version.
3. Disable the related Kconfig option or update the wrapper/submodule pair together.

### Trace build path requested

The trace option is intentionally unavailable by default. Do not force-enable trace until `CO_trace.c/.h` are ported to the current SDO server and OD APIs used by this package.

### Local changes inside `CANopenNode/`

Before updating the submodule, inspect local changes:

```sh
cd CANopenNode
git status
```

Do not overwrite local submodule changes unless they are intentionally discarded or committed in the submodule workflow.
