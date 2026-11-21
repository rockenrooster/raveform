package dev.raveform.game;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageInstaller;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.widget.Toast;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.HashSet;
import java.util.Set;

/**
 * The in-app updater (2026-11-21; Trueform's, docs/in-app-updater.md in that project): downloads the latest GitHub release's
 * APK, checks it (size, SHA-256, same app, same signing key, newer version code) and hands it to Android's installer, which
 * asks the user to confirm. No browser, no storage permission.
 */
public final class ApkUpdater {
    public interface Listener {
        void onProgress(float fraction);   // 0..1 while downloading
        void onFailed(String reason);
    }

    private interface Progress { void bytes(long n); }

    private static final Handler MAIN = new Handler(Looper.getMainLooper());
    private static volatile boolean busy;
    private static volatile Listener listener;
    private static volatile Context appContext;

    /** repo "owner/name"; asset "game.apk" (its checksum is "game.apk.sha256" in the same release). */
    public static void downloadAndInstall(Activity activity, String repo, String asset, Listener l) {
        listener = l;
        appContext = activity.getApplicationContext();
        if (!activity.getPackageManager().canRequestPackageInstalls()) {   // once: allow, come back, tap again
            activity.startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                    Uri.parse("package:" + activity.getPackageName())));
            return;
        }
        if (busy) return;
        busy = true;
        final Context app = appContext;
        new Thread(() -> {
            File dir = new File(app.getCacheDir(), "updates");
            File apk = new File(dir, asset);
            try {
                File[] old = dir.listFiles();
                if (old != null) for (File f : old) f.delete();
                dir.mkdirs();
                JSONObject release = new JSONObject(new String(
                        get("https://api.github.com/repos/" + repo + "/releases/latest", 1 << 20), StandardCharsets.UTF_8));
                String apkUrl = null, shaUrl = null;
                long size = 0;
                JSONArray assets = release.getJSONArray("assets");
                for (int i = 0; i < assets.length(); i++) {
                    JSONObject a = assets.getJSONObject(i);
                    if (asset.equals(a.optString("name"))) { apkUrl = a.optString("browser_download_url"); size = a.optLong("size"); }
                    if ((asset + ".sha256").equals(a.optString("name"))) shaUrl = a.optString("browser_download_url");
                }
                if (apkUrl == null || shaUrl == null) throw new IOException("the release has no " + asset + " or no checksum");
                String want = new String(get(shaUrl, 4096), StandardCharsets.UTF_8).trim().split("\\s+")[0].toLowerCase();
                final long total = size;
                download(apkUrl, apk, n -> { if (total > 0) MAIN.post(() -> l.onProgress(Math.min(1f, (float) n / total))); });
                if (total > 0 && apk.length() != total) throw new IOException("the download is incomplete");
                if (!sha256(apk).equals(want)) throw new IOException("the checksum does not match");
                String why = verify(app, apk);
                if (why != null) throw new IOException(why);
                install(app, apk);   // the installer's dialog follows through ApkInstallReceiver
            } catch (Exception e) {
                apk.delete();
                failed(e.getMessage() != null ? e.getMessage() : e.getClass().getSimpleName());
            } finally {
                busy = false;
            }
        }, "apk-updater").start();
    }

    /** The app's start: the last attempt's download goes (installed or not, a release is about 8 MB in the cache). */
    static void cleanup(Context c) {
        if (busy) return;
        File[] old = new File(c.getCacheDir(), "updates").listFiles();
        if (old != null) for (File f : old) f.delete();
    }

    /** Also called by ApkInstallReceiver when the installer gives up. */
    static void failed(String reason) {
        MAIN.post(() -> {
            Listener l = listener;
            if (l != null) l.onFailed(reason);
            else if (appContext != null) Toast.makeText(appContext, "Update not installed: " + reason, Toast.LENGTH_LONG).show();
        });
    }

    /** null when the file is this app, signed with this app's key and newer; otherwise the reason. */
    @SuppressWarnings("deprecation")
    static String verify(Context c, File apk) throws PackageManager.NameNotFoundException {
        PackageManager pm = c.getPackageManager();
        int flags = Build.VERSION.SDK_INT >= 28 ? PackageManager.GET_SIGNING_CERTIFICATES : PackageManager.GET_SIGNATURES;
        PackageInfo theirs = pm.getPackageArchiveInfo(apk.getPath(), flags);
        if (theirs == null) return "not an app package";
        if (!c.getPackageName().equals(theirs.packageName)) return "it is another app (" + theirs.packageName + ")";
        PackageInfo mine = pm.getPackageInfo(c.getPackageName(), flags);
        if (version(theirs) <= version(mine)) return "it is not newer than this build";
        Set<String> a = signers(theirs), b = signers(mine);
        if (a.isEmpty() || !a.equals(b)) return "it is signed with another key";
        return null;
    }

    @SuppressWarnings("deprecation")
    private static long version(PackageInfo p) { return Build.VERSION.SDK_INT >= 28 ? p.getLongVersionCode() : p.versionCode; }

    @SuppressWarnings("deprecation")
    private static Set<String> signers(PackageInfo p) {
        Signature[] s = Build.VERSION.SDK_INT >= 28 ? (p.signingInfo != null ? p.signingInfo.getApkContentsSigners() : null) : p.signatures;
        Set<String> out = new HashSet<>();
        if (s != null) for (Signature x : s) out.add(x.toCharsString());
        return out;
    }

    private static void install(Context c, File apk) throws IOException {
        PackageInstaller installer = c.getPackageManager().getPackageInstaller();
        PackageInstaller.SessionParams params = new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
        params.setAppPackageName(c.getPackageName());
        if (Build.VERSION.SDK_INT >= 31) params.setRequireUserAction(PackageInstaller.SessionParams.USER_ACTION_REQUIRED);
        int id = installer.createSession(params);
        try (PackageInstaller.Session session = installer.openSession(id)) {
            try (OutputStream out = session.openWrite("update.apk", 0, apk.length()); InputStream in = new FileInputStream(apk)) {
                byte[] buf = new byte[64 * 1024];
                for (int n; (n = in.read(buf)) > 0; ) out.write(buf, 0, n);
                session.fsync(out);
            }
            int flags = PendingIntent.FLAG_UPDATE_CURRENT | (Build.VERSION.SDK_INT >= 31 ? PendingIntent.FLAG_MUTABLE : 0);
            PendingIntent done = PendingIntent.getBroadcast(c, id, new Intent(c, ApkInstallReceiver.class), flags);
            session.commit(done.getIntentSender());
        }
    }

    private static HttpURLConnection open(String url) throws IOException {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setConnectTimeout(8000);
        c.setReadTimeout(8000);
        c.setInstanceFollowRedirects(true);
        c.setRequestProperty("User-Agent", "apk-updater");
        return c;
    }

    private static byte[] get(String url, int limit) throws IOException {
        HttpURLConnection c = open(url);
        c.setRequestProperty("Accept", "application/vnd.github+json");
        try {
            if (c.getResponseCode() / 100 != 2) throw new IOException("HTTP " + c.getResponseCode());
            try (InputStream in = c.getInputStream()) {
                ByteArrayOutputStream out = new ByteArrayOutputStream();
                byte[] buf = new byte[16 * 1024];
                for (int n; (n = in.read(buf)) > 0; ) {
                    out.write(buf, 0, n);
                    if (out.size() > limit) throw new IOException("response too large");
                }
                return out.toByteArray();
            }
        } finally {
            c.disconnect();
        }
    }

    private static void download(String url, File to, Progress progress) throws IOException {
        HttpURLConnection c = open(url);
        c.setRequestProperty("Accept", "application/octet-stream");
        try {
            if (c.getResponseCode() / 100 != 2) throw new IOException("HTTP " + c.getResponseCode());
            try (InputStream in = c.getInputStream(); OutputStream out = new FileOutputStream(to)) {
                byte[] buf = new byte[64 * 1024];
                long total = 0;
                for (int n; (n = in.read(buf)) > 0; ) { out.write(buf, 0, n); total += n; progress.bytes(total); }
            }
        } finally {
            c.disconnect();
        }
    }

    private static String sha256(File f) throws Exception {
        MessageDigest md = MessageDigest.getInstance("SHA-256");
        try (InputStream in = new FileInputStream(f)) {
            byte[] buf = new byte[64 * 1024];
            for (int n; (n = in.read(buf)) > 0; ) md.update(buf, 0, n);
        }
        StringBuilder hex = new StringBuilder();
        for (byte x : md.digest()) hex.append(String.format("%02x", x));
        return hex.toString();
    }
}
