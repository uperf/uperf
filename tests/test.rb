#!/bin/env ruby -w

ENV["h"]="localhost"
ENV["h1"]="localhost"
ENV["h2"]="localhost"
ENV["h3"]="localhost"
ENV["h4"]="localhost"
ENV["nthr"]="10"
ENV["conn"]="10"
ENV["proto"]="tcp"
ENV["iter"]="100"
ENV["size"]="1400"
ENV["t"]="10s"
ENV["rate"]="1000"

arg = ARGV[0] || "tests/*xml"
pass=[]
fail=[]
cmd = "framework/uperf"
#log = File.open("log","w+")
####Dir['tests/*xml'].each do |file|
Dir[arg].each do |file|
  cmd1="#{cmd} -m #{file}"
  #IO.popen (cmd1) { |f| line = f.gets; puts(line); log.puts f }
  puts cmd1
system("echo #{cmd1} >> log")
system("#{cmd1} >> log")
  if $? == 0 then
    pass << file
  else
    fail << file
  end
end
puts "Pass"
pass.each{|r| puts(r)}
puts "Failed"
fail.each{|r| puts(r)}
