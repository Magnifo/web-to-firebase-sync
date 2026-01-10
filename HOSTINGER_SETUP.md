# Hostinger Cron Job Setup Guide

This guide explains how to set up a reliable cron job on Hostinger to trigger your GitHub Actions workflow every 10 minutes.

## Why Use Hostinger Cron Instead of GitHub Actions Schedule?

GitHub Actions scheduled workflows can be unreliable:
- ❌ Delays of hours are common during high load
- ❌ No guarantee of exact timing
- ❌ Can be skipped entirely during outages

Hostinger cron jobs:
- ✅ Run at exact specified intervals
- ✅ Reliable and predictable
- ✅ Better for time-sensitive data sync

---

## Step 1: Create GitHub Personal Access Token (PAT)

1. Go to GitHub Settings → Developer settings → Personal access tokens → Tokens (classic)
   - URL: https://github.com/settings/tokens

2. Click **Generate new token** → **Generate new token (classic)**

3. Configure the token:
   - **Note**: `Hostinger Cron Trigger`
   - **Expiration**: Choose your preference (recommend 90 days or No expiration)
   - **Scopes**: Check **only** these:
     - ✅ `repo` (Full control of private repositories)
       - This includes the `repo:status`, `repo_deployment`, and `public_repo` subscopes

4. Click **Generate token**

5. **IMPORTANT**: Copy the token immediately (starts with `ghp_...`)
   - You won't be able to see it again!

---

## Step 2: Configure the PHP Script

1. Open `trigger-sync.php` file

2. Replace these values:
   ```php
   $githubUsername = 'YOUR_GITHUB_USERNAME';  // e.g., 'janshahid'
   $repoName = 'web-to-firebase-sync';         // Your repo name
   $githubToken = 'ghp_xxxxxxxxxxxxxxxxxxxx';  // Paste your PAT here
   ```

3. Save the file

---

## Step 3: Upload to Hostinger

1. Log in to your Hostinger control panel

2. Go to **File Manager** or use FTP client

3. Upload `trigger-sync.php` to your website directory
   - Recommended location: `public_html/cron/` (create folder if needed)
   - Full path example: `public_html/cron/trigger-sync.php`

4. Set file permissions to **644** (read/write for owner, read for others)

---

## Step 4: Set Up Cron Job in Hostinger

1. In Hostinger control panel, go to **Advanced** → **Cron Jobs**

2. Click **Create new cron job**

3. Configure the cron job:
   - **Interval**: Select **Every 10 minutes**
     - Or use custom: `*/10 * * * *`
   
   - **Command**: Enter one of these options:
   
     **Option A: Using PHP CLI** (Recommended)
     ```bash
     /usr/bin/php /home/USERNAME/public_html/cron/trigger-sync.php
     ```
     
     **Option B: Using wget**
     ```bash
     wget -q -O /dev/null https://yourdomain.com/cron/trigger-sync.php
     ```
     
     **Option C: Using curl**
     ```bash
     curl -s https://yourdomain.com/cron/trigger-sync.php
     ```

   - **Email notifications**: Enable to get alerts if the cron fails

4. Click **Create**

> **Note**: Replace `USERNAME` with your Hostinger username and `yourdomain.com` with your actual domain.

---

## Step 5: Test the Setup

### Test Manually First

1. Visit the script directly in your browser:
   ```
   https://yourdomain.com/cron/trigger-sync.php
   ```

2. You should see:
   ```
   ✓ Workflow triggered successfully at 2026-01-10 12:50:00
   ```

3. Check GitHub Actions:
   - Go to your repository → **Actions** tab
   - You should see a new workflow run triggered by `repository_dispatch`

### Check Logs

1. Check the cron trigger log file:
   - Location: `public_html/cron/cron-trigger.log`
   - Should show successful HTTP 204 responses

2. Check Hostinger cron logs:
   - In Hostinger panel, go to **Cron Jobs** → View logs for your cron job

---

## Step 6: Optional - Remove GitHub Schedule Trigger

Once you confirm the Hostinger cron is working, you can remove the unreliable GitHub schedule:

Edit `.github/workflows/sync.yml` and remove these lines:
```yaml
  # Optional fallback (can remove later)
  schedule:
    - cron: '*/10 * * * *'
```

Keep these triggers:
- `repository_dispatch` - For Hostinger cron
- `workflow_dispatch` - For manual testing

---

## Troubleshooting

### Issue: "401 Unauthorized" or "403 Forbidden"

**Solution**: 
- Check that your GitHub token is correct
- Ensure the token has `repo` scope
- Verify the repository name and username are correct

### Issue: "404 Not Found"

**Solution**:
- Verify repository name: `githubUsername/repoName` format
- Check if repository is private (requires full `repo` scope)

### Issue: Cron not running

**Solution**:
- Check cron job status in Hostinger panel
- Verify file path in cron command is absolute
- Check file permissions (should be 644)
- Enable email notifications to get error reports

### Issue: Script runs but workflow doesn't trigger

**Solution**:
- Check `event_type` matches in both `trigger-sync.php` and `sync.yml`
- Currently set to: `hostinger_cron`
- Verify GitHub token permissions

---

## Security Best Practices

1. **Protect the PHP file from public access**:
   
   Create `.htaccess` in the same directory:
   ```apache
   # Deny access from web browsers
   <Files "trigger-sync.php">
       Order Allow,Deny
       Deny from all
   </Files>
   ```
   
   Only allow cron to run it via PHP CLI

2. **Don't commit the token**:
   - Add to `.gitignore`: `trigger-sync.php`
   - Or store token in environment variable

3. **Rotate tokens regularly**:
   - Update your GitHub PAT every 90 days

---

## Monitoring

Check these regularly:
- Hostinger cron execution logs
- `cron-trigger.log` file
- GitHub Actions workflow runs
- Firebase data updates

---

## Expected Behavior

✅ **Every 10 minutes**:
1. Hostinger cron runs `trigger-sync.php`
2. Script sends API request to GitHub
3. GitHub triggers `repository_dispatch` event
4. Workflow runs and syncs flight data
5. Results logged in `cron-trigger.log`

---

## Need Help?

If you encounter issues:
1. Check the log file: `cron-trigger.log`
2. Test the script manually in browser
3. Verify GitHub Actions workflow runs
4. Check Hostinger cron job logs
