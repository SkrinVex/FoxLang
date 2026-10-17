// Show only GUI assets actually present in the latest stable release.
(async () => {
  const status = document.getElementById('download-status');
  if (!status) return;
  try {
    const response = await fetch('https://api.github.com/repos/SkrinVex/Skrumi/releases/latest',
      {headers: {Accept: 'application/vnd.github+json'}, signal: AbortSignal.timeout(10000)});
    if (!response.ok) throw Error('Release lookup failed');
    const release = await response.json();
    if (release.draft || release.prerelease || !Array.isArray(release.assets)) throw Error('Invalid stable release');
    let count = 0;
    for (const [platform, name] of [['windows', 'Skrumi-windows-x86_64.exe'], ['linux', 'Skrumi-linux-x86_64.run']]) {
      const asset = release.assets.find(a => a.name === name && a.state === 'uploaded' && a.size > 0);
      if (!asset) continue;
      const url = new URL(asset.browser_download_url);
      if (url.origin !== 'https://github.com' || !url.pathname.startsWith('/SkrinVex/Skrumi/releases/download/')) continue;
      const link = document.getElementById(`download-${platform}`);
      link.href = url.href;
      link.hidden = false;
      count++;
    }
    status.textContent = count ? `Стабильный релиз ${release.tag_name}. Доступные установщики показаны выше.` :
      'В последнем стабильном релизе GUI-установщиков ещё нет. Доступна сборка из исходников; подробности на GitHub.';
  } catch {
    status.textContent = 'Не удалось проверить файлы релиза. Проверьте доступные загрузки на GitHub Releases.';
  }
})();
