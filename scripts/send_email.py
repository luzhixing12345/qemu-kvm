import subprocess
from datetime import datetime
import pytz
import os

def get_git_user_info():
    """Get the user name and email from git config."""
    try:
        name = subprocess.check_output(['git', 'config', '--get', 'user.name']).strip().decode('utf-8')
        email = subprocess.check_output(['git', 'config', '--get', 'user.email']).strip().decode('utf-8')
        return name, email
    except subprocess.CalledProcessError:
        return None, None

def get_current_date():
    """Get the current date and time in the specified format."""
    tz = pytz.timezone('Asia/Shanghai')  # Set the timezone to your desired timezone
    now = datetime.now(tz)
    return now.strftime('%a, %d %b %Y %H:%M:%S %z')

def generate_patch_email():
    """Generate the patch email content."""
    name, email = get_git_user_info()
    if not name or not email:
        print("Error: Unable to get git user info.")
        return

    current_date = get_current_date()
    
    email_content = f"""From: {name} <{email}>
Date: {current_date}
Subject: Re: [PATCH]

Signed-off-by: {name} <{email}>
"""
    return email_content

if __name__ == '__main__':
    patch_email = generate_patch_email()
    reply_patch_name = 'reply.patch'
    if os.path.exists(reply_patch_name):
        print(f"Error: {reply_patch_name} already exists.")
        exit(1)

    with open('reply.patch', 'w') as f:
        f.write(patch_email)
    print(f"Generated patch email: {reply_patch_name}")