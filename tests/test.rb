#!/bin/env ruby -w

ENV["h"]="frost"
ENV["h1"]="frost"
ENV["h2"]="frost"
ENV["h3"]="frost"
ENV["h4"]="frost"
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
cmd = "src/uperf"
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
