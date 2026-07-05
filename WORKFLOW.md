# 博客工作流手册

## 目录结构

```
D:\RDMA\
├── 24-month-plan.md.md          # 24 个月转型计划
├── blog\                         # 旧版 HTML 博客（已弃用）
├── blog-site\                    # Hugo 博客源码（新版，活跃）
│   ├── content\posts\            # 你的博客文章（.md 文件）
│   │   └── why-rdma-for-llm.md   # 第一篇博客
│   ├── hugo.toml                 # Hugo 配置文件
│   ├── themes\PaperMod\          # 主题
│   ├── .github\workflows\        # GitHub Actions 自动部署
│   ├── deploy.bat                # 一键部署脚本
│   └── public\                   # Hugo 生成的网站文件（.gitignore 排除）
```

## 发布一篇新博客

### 方式一：一键部署（推荐）

双击 `D:\RDMA\blog-site\deploy.bat`，输入提交说明，自动完成构建+提交+推送。

### 方式二：手动操作

```cmd
:: 1. 新建文章
cd /d D:\RDMA\blog-site
hugo new content posts/文章名.md

:: 2. 编辑文章（用 VS Code 打开 content/posts/文章名.md）
code content/posts/文章名.md

:: 3. 设置代理并推送
set HTTP_PROXY=http://localhost:15236
set HTTPS_PROXY=http://localhost:15236
git add -A
git commit -m "新文章：文章标题"
git push
```

## 文章头部格式（Front Matter）

每篇 `.md` 文件开头必须包含：

```yaml
---
title: '文章标题'
date: 2026-07-05
draft: false
series: '从 AP 到 AI 网络'     ← 系列名固定，自动归类
weight: 2                       ← 系列内排序（1, 2, 3...）
tags: ['RDMA', 'AI 网络']      ← 标签
description: '文章简介，会显示在首页列表'
---
```

## 本地预览

```cmd
cd /d D:\RDMA\blog-site
hugo server --buildDrafts
```
浏览器打开 http://localhost:1313

## 部署地址

博客公网地址：https://Jeffrey-Hu-171778195.github.io/

## 注意

- 推送需代理：梯子监听 localhost:15236
- SSH 方式不生效，梯子不转发 22 端口
- 文章发布后等待 1-2 分钟 GitHub Pages 生效
- M5 之前只需博客仓库，后续 RDMA 项目另建仓库