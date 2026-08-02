# Публикация на GitHub

Предлагаемое имя репозитория:

```text
AIDevelopersMonster/T-Internet-COM-Lab
```

## Вариант 1 — из подготовленной папки

После создания пустого репозитория на GitHub:

```bash
cd T-Internet-COM-Lab
git remote add origin https://github.com/AIDevelopersMonster/T-Internet-COM-Lab.git
git push -u origin main
```

Не добавляйте при создании репозитория отдельные README, `.gitignore` или LICENSE: они уже находятся в проекте.

## Вариант 2 — из Git bundle

```bash
git clone T-Internet-COM-Lab.bundle T-Internet-COM-Lab
cd T-Internet-COM-Lab
git remote add origin https://github.com/AIDevelopersMonster/T-Internet-COM-Lab.git
git push -u origin main
```

Bundle содержит ветку `main` и начальный коммит проекта.
