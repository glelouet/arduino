# -*- coding: utf-8 -*-
# Generated by Django 1.9.1 on 2016-02-02 09:48
from __future__ import unicode_literals

from django.db import migrations, models
import django.db.models.deletion


class Migration(migrations.Migration):

    dependencies = [
        ('website', '0001_initial'),
    ]

    operations = [
        migrations.CreateModel(
            name='Channel',
            fields=[
                ('name', models.CharField(max_length=200)),
                ('API_KEY', models.CharField(max_length=16, primary_key=True, serialize=False)),
            ],
        ),
        migrations.CreateModel(
            name='Sensor',
            fields=[
                ('name', models.CharField(max_length=200, primary_key=True, serialize=False)),
                ('bridge', models.ForeignKey(null=True, on_delete=django.db.models.deletion.SET_NULL, to='website.Bridge')),
                ('user', models.ForeignKey(on_delete=django.db.models.deletion.CASCADE, to='website.User')),
            ],
        ),
        migrations.AddField(
            model_name='channel',
            name='sensors',
            field=models.ManyToManyField(to='website.Sensor'),
        ),
        migrations.AddField(
            model_name='channel',
            name='user',
            field=models.ForeignKey(on_delete=django.db.models.deletion.CASCADE, to='website.User'),
        ),
    ]
