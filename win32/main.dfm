object rfMain: TrfMain
  Left = 0
  Top = 0
  BorderStyle = bsNone
  Caption = 'fsplayer'
  ClientHeight = 338
  ClientWidth = 651
  Color = clBlack
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'Tahoma'
  Font.Style = []
  OldCreateOrder = False
  WindowState = wsMaximized
  OnClose = FormClose
  OnCreate = FormCreate
  OnKeyDown = FormKeyDown
  PixelsPerInch = 96
  TextHeight = 13
  object rrView: TPanel
    Left = 104
    Top = 64
    Width = 385
    Height = 217
    BevelOuter = bvNone
    DoubleBuffered = False
    FullRepaint = False
    ParentBackground = False
    ParentDoubleBuffered = False
    ShowCaption = False
    TabOrder = 0
  end
  object rfOpen: TOpenDialog
    Filter = 'mp4 files|*.mp4|mkv files|*.mkv|avi files|*.avi|All files|*.*'
    Options = [ofEnableSizing]
    Title = 'fsplayer - Open Video'
    Left = 8
    Top = 8
  end
  object rrTimer: TTimer
    Interval = 400
    OnTimer = rrTimerTimer
    Left = 56
    Top = 8
  end
end
